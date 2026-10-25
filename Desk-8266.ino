#include <Arduino.h>
#include "Config.h"
#include "MotionSensor.h"
#include "EnvironmentSensor.h"
#include "NetworkTime.h"
#include "DisplayRenderer.h"

// Core System Modules
DisplayRenderer   renderer;
MotionSensor      motionSensor;
EnvironmentSensor envSensor;
NetworkTime       networkTime;

AppMode currentMode = AppMode::Clock;
bool    isTouched   = false;

unsigned long lastFrameTime = 0;

void setup() {
  // Serial communication is intentionally omitted to reserve GPIO 1 (TX) for DHT22 data
  pinMode(PIN_TOUCH_TTP223, INPUT);

  renderer.begin();
  motionSensor.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  envSensor.begin(PIN_DHT22_DATA);

  // Initialize WiFi connection and synchronize time via NTP
  networkTime.begin([](const char* statusText, int percent, const char* detailText) {
    renderer.showWiFiProgress(statusText, percent, detailText);
  });

  renderer.renderStaticBorder();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrameTime < 16) {
    yield();
    return;
  }
  lastFrameTime = now;

  // Touch sensor input with 250ms rising-edge debounce to toggle operating modes
  bool touchRaw = (digitalRead(PIN_TOUCH_TTP223) == HIGH);
  isTouched = touchRaw;

  static bool prevTouchRaw = false;
  static unsigned long lastTouchToggleMs = 0;

  if (touchRaw && !prevTouchRaw && (now - lastTouchToggleMs > 250)) {
    lastTouchToggleMs = now;
    currentMode = (currentMode == AppMode::Clock) ? AppMode::Sensors : AppMode::Clock;
  }
  prevTouchRaw = touchRaw;

  // Sensor updates
  motionSensor.update();
  envSensor.update();

  // Retrieve current time with sub-second resolution for animation smoothing
  int hour = 0;
  int minute = 0;
  int second = 0;
  unsigned long subSecondMs = 0;
  networkTime.getCurrentTime(hour, minute, second, subSecondMs);

  // Render active mode frame
  float pitch = motionSensor.isAvailable() ? motionSensor.getFilteredPitch() : 0.0f;
  renderer.renderFrame(pitch, isTouched, envSensor.getTemperature(), envSensor.getHumidity(),
                       hour, minute, second, subSecondMs, currentMode);
  yield();
}
