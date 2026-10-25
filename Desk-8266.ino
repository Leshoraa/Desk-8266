#include <Arduino.h>
#include "Config.h"
#include "MotionSensor.h"
#include "EnvironmentSensor.h"
#include "NetworkTime.h"
#include "WeatherService.h"
#include "DisplayRenderer.h"

// Core System Modules
DisplayRenderer   renderer;
MotionSensor      motionSensor;
EnvironmentSensor envSensor;
NetworkTime       networkTime;
WeatherService    weatherService;

AppMode userMode          = AppMode::Clock;
bool    isPeeking         = false;
unsigned long lastPeekCycleMs = 0;
unsigned long peekStartMs     = 0;
bool    isTouched         = false;

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

  // Touch sensor input with release-based double-tap and hold detection:
  // - Hold (>=500ms): In Main Settings, enters Sub-Menu for current setting without advancing.
  // - Tap (Release <500ms): Confirmed 280ms after release. Single tap toggles Clock <-> Sensors or advances setting/value.
  // - Double Tap: Two releases within 280ms. Enters Settings from Clock/Sensors; exits Sub-Menu; exits Settings to Clock.
  bool touchRaw = (digitalRead(PIN_TOUCH_TTP223) == HIGH);
  isTouched = touchRaw; 

  static bool prevTouchRaw = true; // Assume high during boot settling to avoid false startup toggle
  static unsigned long lastTouchDownMs = 2000;
  static unsigned long lastTouchReleaseMs = 0;
  static bool pendingTap = false;
  static bool holdTriggered = false;

  bool touchPressed  = (touchRaw && !prevTouchRaw && (now - lastTouchDownMs > 120));
  bool touchReleased = (!touchRaw && prevTouchRaw);

  // 1. Finger Pressed Down (Rising Edge)
  if (touchPressed) {
    lastTouchDownMs = now;
    holdTriggered = false;
    isPeeking = false;
    lastPeekCycleMs = now;
  }

  // 2. Continuous Hold Detection (Finger held down continuously >= 500ms)
  if (touchRaw && !holdTriggered && (now - lastTouchDownMs >= 500)) {
    holdTriggered = true;
    pendingTap = false; // Cancel any pending tap so setting NEVER advances while holding!

    if (userMode == AppMode::Settings && !renderer.isInSubMenu()) {
      // Hold on active main setting: enter sub-menu!
      renderer.enterSubMenu();
      if (renderer.isSettingsExitRequested()) {
        userMode = AppMode::Clock;
        renderer.resetSettingsExit();
      }
    }
  }

  // 3. Finger Released (Falling Edge)
  if (touchReleased) {
    if (holdTriggered) {
      // Finger released after hold was already processed: do nothing
      holdTriggered = false;
    } else {
      // Finger released before hold threshold: this is a CLICK!
      if (pendingTap && (now - lastTouchReleaseMs < 280)) {
        // DOUBLE TAP DETECTED (< 280ms between releases)!
        pendingTap = false;

        if (userMode == AppMode::Settings) {
          if (renderer.isInSubMenu()) {
            // Double tap in Sub-Menu: return to Main Settings
            renderer.exitSubMenu();
          } else if (renderer.isCurrentSettingExit()) {
            // Double tap on item 7 (EXIT): exit to Clock!
            userMode = AppMode::Clock;
            renderer.resetSettingsExit();
          } else {
            // Double tap while browsing items 1-6: advance safely (prevent accidental exit to Clock)
            renderer.advanceSettings();
          }
        } else {
          // Double tap in Clock or Sensors: enter Settings!
          userMode = AppMode::Settings;
          renderer.openSettings();
        }
      } else {
        // First click release: arm single tap confirmation window
        pendingTap = true;
        lastTouchReleaseMs = now;
      }
    }
  }

  // 4. Confirm Single Tap when 280ms expires after release while finger is NOT touching
  if (pendingTap && !touchRaw && (now - lastTouchReleaseMs >= 280)) {
    pendingTap = false;

    if (userMode == AppMode::Settings) {
      if (renderer.isInSubMenu()) {
        // Single tap in Sub-Menu: advance value (10% -> 40% -> 60% -> 100%)
        renderer.advanceSubMenu();
      } else {
        // Single tap in Main Settings: advance rotary wheel
        renderer.advanceSettings();
        if (renderer.isSettingsExitRequested()) {
          userMode = AppMode::Clock;
          renderer.resetSettingsExit();
        }
      }
    } else if (userMode == AppMode::Clock) {
      userMode = AppMode::Sensors;
    } else if (userMode == AppMode::Sensors) {
      userMode = AppMode::Clock;
    }
  }

  // 5. Settings Inactivity Timeout: 15s idle in Settings returns to Clock
  if (userMode == AppMode::Settings && renderer.getLastSettingTouchMs() > 0 && (now - renderer.getLastSettingTouchMs() > 15000)) {
    userMode = AppMode::Clock;
  }
  prevTouchRaw = touchRaw;

  // Sensor and weather updates
  motionSensor.update();
  envSensor.update();
  weatherService.update();

  float pitch = (motionSensor.isAvailable() && renderer.isGyroTrackingEnabled()) ? motionSensor.getFilteredPitch() : 0.0f;
  bool isNightStand = renderer.isNightStandEnabled() && (pitch >= NIGHT_STAND_PITCH_MIN && pitch <= NIGHT_STAND_PITCH_MAX);

  // Auto-Glance: Periodically peek reciprocal mode (Clock -> Sensors) when enabled
  if (!isNightStand && userMode != AppMode::Settings && renderer.isAutoPeekEnabled()) {
    if (lastPeekCycleMs == 0) {
      lastPeekCycleMs = now;
    }
    if (!isPeeking) {
      if (now - lastPeekCycleMs >= AUTO_PEEK_INTERVAL_MS) {
        isPeeking = true;
        peekStartMs = now;
      }
    } else {
      if (now - peekStartMs >= AUTO_PEEK_DURATION_MS) {
        isPeeking = false;
        lastPeekCycleMs = now;
      }
    }
  } else {
    isPeeking = false;
    lastPeekCycleMs = now;
  }

  AppMode activeMode = userMode;
  if (isPeeking) {
    activeMode = (userMode == AppMode::Clock) ? AppMode::Sensors : AppMode::Clock;
  }

  // Retrieve current time with sub-second resolution for animation smoothing and calendar date
  int hour = 0;
  int minute = 0;
  int second = 0;
  unsigned long subSecondMs = 0;
  char dateText[12] = "";
  networkTime.getCurrentTime(hour, minute, second, subSecondMs, dateText, sizeof(dateText));

  // Network diagnostics cached specifically for Mode 3 (zero heap allocations on hot path)
  static char cachedIp[16] = "";
  static int  cachedRssi = 0;
  static unsigned long lastNetDiagMs = 0;

  if (activeMode == AppMode::Settings && (now - lastNetDiagMs > 3000 || lastNetDiagMs == 0)) {
    lastNetDiagMs = now;
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      snprintf(cachedIp, sizeof(cachedIp), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      cachedRssi = WiFi.RSSI();
    } else {
      strncpy(cachedIp, "Offline", sizeof(cachedIp));
      cachedRssi = 0;
    }
  }

  // Render active mode frame
  renderer.renderFrame(pitch, isTouched, envSensor.getTemperature(), envSensor.getHumidity(),
                       hour, minute, second, subSecondMs, dateText, weatherService.getCondition(),
                       activeMode, (activeMode == AppMode::Settings) ? cachedIp : nullptr, cachedRssi);
  yield();
}
