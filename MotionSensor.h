#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

class MotionSensor {
public:
  MotionSensor();

  void begin(uint8_t sdaPin, uint8_t sclPin);
  void update();
  bool isAvailable() const;
  float getFilteredPitch() const;

private:
  bool wakeDevice(uint8_t address);
  void scanAndInit();

  uint8_t deviceAddress;
  bool deviceAvailable;
  float currentPitchDeg;
  unsigned long lastRetryTimestamp;
};

#endif // MOTION_SENSOR_H
