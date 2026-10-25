#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "Config.h"
#include "weather_icons.h"

class DisplayRenderer {
public:
  DisplayRenderer();

  void begin();
  void showWiFiProgress(const char* statusText, int percent, const char* detailText);
  void renderStaticBorder();
  void renderFrame(float pitchDeg, bool isTouched, float temperature, float humidity,
                   int hour, int minute, int second, unsigned long subSecondMs,
                   const char* dateText, WeatherType weatherType, AppMode mode,
                   const char* wifiIp = nullptr, int wifiRssi = 0);

  void openSettings();
  void advanceSettings();
  bool isInSubMenu() const { return inSubMenu; }
  void enterSubMenu();
  void advanceSubMenu();
  void exitSubMenu();
  bool isSettingsExitRequested() const { return isExitRequested; }
  void resetSettingsExit() { isExitRequested = false; }
  bool isCurrentSettingExit() const { return currentSettingIndex == (SETTINGS_NUM_ITEMS - 1); }
  bool isAutoPeekEnabled() const { return autoPeekEnabled; }
  bool isNightStandEnabled() const { return nightStandEnabled; }
  bool isGyroTrackingEnabled() const { return gyroTrackingEnabled; }
  uint16_t getThemeAccentColor() const;
  uint8_t getUserBrightness() const { return userBrightness; }
  unsigned long getLastSettingTouchMs() const { return lastSettingTouchMs; }

private:
  void drawLineToBand(int x0, int y0, int x1, int y1, int yStart, uint16_t color);
  void drawCenterCapToBand(int cx, int cy, int yStart, uint16_t color, uint16_t holeColor);
  void drawCircleToBand(int cx, int cy, int radius, int yStart, uint16_t color);
  void drawCircleOutlineToBand(int cx, int cy, int radius, int yStart, uint16_t color);
  void drawCapsuleToBand(int cx, int cy, int hw, int hh, float angleRad, int yStart, uint16_t color);
  void drawRotatedStringToBand(int cx, int cy, const char* str, float angleRad, uint8_t size, int yStart, uint16_t color);
  void drawRotatedScaledStringToBand(int cx, int cy, const char* str, float angleRad, float scale, int yStart, uint16_t color);
  void drawWeatherIconToBand(int cx, int cy, WeatherType type, float angleRad, float scale, int yStart, uint16_t color);
  void printFlickerFreeCentered(const char* text, int y, uint8_t size, uint16_t color, int totalChars);

  Adafruit_GC9A01A tft;
  uint16_t screenBand[SCREEN_WIDTH * BAND_HEIGHT];

  float lastRenderedPitch;
  bool lastRenderedTouch;
  float lastRenderedTemp;
  float lastRenderedHum;
  AppMode lastRenderedMode;
  unsigned long lastClockRenderMs;

  int lastProgressBarWidth;
  int lastProgressPercent;
  bool isWiFiStaticDrawn;

  // Mode Morphing State (Continuous 3-way smooth transition)
  float clockWeightMorph;
  float sensorWeightMorph;
  float settingsWeightMorph;
  unsigned long lastMorphUpdateMs;

  // Settings Mode (Rotary Selector Dial)
  int currentSettingIndex;
  float currentWheelAngle;
  float targetWheelAngle;
  bool isExitRequested;
  bool inSubMenu;
  uint8_t brightnessStepIndex;
  uint8_t infoSubIndex;
  uint8_t userBrightness;
  bool autoPeekEnabled;
  bool nightStandEnabled;
  bool gyroTrackingEnabled;
  uint8_t themeIndex;
  unsigned long lastSettingTouchMs;

  struct KnurlTick {
    int16_t x1, y1, x2, y2;
    bool isMajor;
  };
  KnurlTick knurlTicks[SETTINGS_KNURL_TICKS];

  // Weather Ambiance Particles
  struct WeatherParticle {
    float dist;
    float speed;
  };
  WeatherParticle weatherParticles[8];
  unsigned long lastWeatherParticleUpdateMs;
  unsigned long lastThunderFlashMs;

  // Night Stand State
  bool isNightStandActive;
  uint8_t currentBacklightLevel;
};

#endif // DISPLAY_RENDERER_H
