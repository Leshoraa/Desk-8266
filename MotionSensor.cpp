#include "MotionSensor.h"
#include "Config.h"

MotionSensor::MotionSensor()
  : deviceAddress(MPU_ADDR_PRIMARY),
    deviceAvailable(false),
    currentPitchDeg(0.0f),
    lastRetryTimestamp(0) {}

void MotionSensor::begin(uint8_t sdaPin, uint8_t sclPin) {
  Wire.begin(sdaPin, sclPin);
  Wire.setClock(400000); // 400kHz fast I2C mode reduces bus occupancy
  scanAndInit();
}

bool MotionSensor::wakeDevice(uint8_t address) {
  Wire.beginTransmission(address);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0x00); // Clear sleep bit to wake internal oscillator
  if (Wire.endTransmission(true) != 0) {
    return false;
  }

  // Configure hardware Digital Low Pass Filter (DLPF) to 20Hz (Register 0x1A CONFIG)
  // Suppresses high-frequency accelerometer noise, mechanical vibrations, and hand micro-tremor
  Wire.beginTransmission(address);
  Wire.write(0x1A); // CONFIG register
  Wire.write(0x04); // DLPF_CFG = 4 (20Hz Accel bandwidth, 8.5ms delay)
  if (Wire.endTransmission(true) != 0) {
    return false;
  }

  Wire.beginTransmission(address);
  Wire.write(0x1C); // ACCEL_CONFIG register
  Wire.write(0x08); // Select +/- 4G full scale range (8192 LSB/g)
  return (Wire.endTransmission(true) == 0);
}

void MotionSensor::scanAndInit() {
  if (wakeDevice(MPU_ADDR_PRIMARY)) {
    deviceAddress = MPU_ADDR_PRIMARY;
    deviceAvailable = true;
  } else if (wakeDevice(MPU_ADDR_SECONDARY)) {
    deviceAddress = MPU_ADDR_SECONDARY;
    deviceAvailable = true;
  } else {
    deviceAvailable = false;
  }
}

bool MotionSensor::isAvailable() const {
  return deviceAvailable;
}

float MotionSensor::getFilteredPitch() const {
  return currentPitchDeg;
}

void MotionSensor::update() {
  unsigned long now = millis();

  if (!deviceAvailable) {
    if (now - lastRetryTimestamp >= 1000) {
      lastRetryTimestamp = now;
      scanAndInit();
    }
    return;
  }

  Wire.beginTransmission(deviceAddress);
  Wire.write(0x3B); // ACCEL_XOUT_H register
  if (Wire.endTransmission(false) != 0) {
    deviceAvailable = false;
    return;
  }

  if (Wire.requestFrom((uint8_t)deviceAddress, (size_t)6, true) != 6) {
    deviceAvailable = false;
    return;
  }

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();

  // If the device lies flat on a desk, gravity acts primarily along Z.
  // Hold current angle when in-plane magnitude is below ~0.15g to prevent random spinning.
  int32_t magSq = (int32_t)ax * ax + (int32_t)ay * ay;
  if (magSq < 1000000L) {
    return;
  }

  // Damping response during external physical shocks (<0.5g or >1.6g total vector)
  float totalGSq = ((float)magSq + (float)az * az) / (8192.0f * 8192.0f);
  bool isShock = (totalGSq < 0.25f || totalGSq > 2.6f);

  float x = (float)ax;
  float y = (float)ay;

  if (SWAP_XY) {
    float temp = x;
    x = y;
    y = temp;
  }
  if (INVERT_X) x = -x;
  if (INVERT_Y) y = -y;

  // Convert in-plane acceleration vector to rotation angle
  float rawAngle = atan2f(x, y) * 57.2958f + ROTATION_OFFSET_DEG;
  while (rawAngle < 0.0f) rawAngle += 360.0f;
  while (rawAngle >= 360.0f) rawAngle -= 360.0f;

  // Hysteresis zero-snap around resting upright angle
  float snapThreshold = (currentPitchDeg == 0.0f) ? (IDLE_ZERO_SNAP + 2.0f) : IDLE_ZERO_SNAP;
  if (rawAngle <= snapThreshold || rawAngle >= (360.0f - snapThreshold)) {
    rawAngle = 0.0f;
  }

  // Shortest angular path around circular boundary
  float delta = rawAngle - currentPitchDeg;
  while (delta > 180.0f) delta -= 360.0f;
  while (delta < -180.0f) delta += 360.0f;

  float absDelta = fabsf(delta);
  if (absDelta >= IDLE_MOTION_DEADBAND) {
    // Continuous deadband subtraction: ensures zero step discontinuity at threshold
    float effectiveDelta = delta - ((delta > 0.0f) ? IDLE_MOTION_DEADBAND : -IDLE_MOTION_DEADBAND);

    // Adaptive speed curve: gentle damping for small hand wobbles, instant 1:1 tracking for deliberate turns
    float speedFactor = 0.35f;
    if (absDelta > 25.0f) {
      speedFactor = 0.85f;
    } else if (absDelta > 8.0f) {
      speedFactor = 0.60f;
    }

    if (isShock) {
      speedFactor *= 0.3f;
    }

    float step = effectiveDelta * speedFactor;
    if (step > MAX_SPEED_DEG_FRAME) step = MAX_SPEED_DEG_FRAME;
    if (step < -MAX_SPEED_DEG_FRAME) step = -MAX_SPEED_DEG_FRAME;

    currentPitchDeg += step;
  } else if (rawAngle == 0.0f && absDelta > 0.05f) {
    currentPitchDeg += delta * 0.25f;
    if (fabsf(currentPitchDeg) < 0.2f || currentPitchDeg > 359.8f) {
      currentPitchDeg = 0.0f;
    }
  }

  if (currentPitchDeg < 0.0f) currentPitchDeg += 360.0f;
  if (currentPitchDeg >= 360.0f) currentPitchDeg -= 360.0f;
}
