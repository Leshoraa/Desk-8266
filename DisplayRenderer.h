#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "Config.h"

class DisplayRenderer {
public:
  DisplayRenderer();

  void begin();
  void showWiFiProgress(const char* statusText, int percent, const char* detailText);
  void renderStaticBorder();
  void renderFrame(float pitchDeg, bool isTouched, float temperature, float humidity,
                   int hour, int minute, int second, unsigned long subSecondMs, AppMode mode);

private:
  void drawLineToBand(int x0, int y0, int x1, int y1, int yStart, uint16_t color);
  void drawCenterCapToBand(int cx, int cy, int yStart, uint16_t color);
  void drawRotatedStringToBand(int cx, int cy, const char* str, float angleRad, uint8_t size, int yStart, uint16_t color);
  void drawRotatedScaledStringToBand(int cx, int cy, const char* str, float angleRad, float scale, int yStart, uint16_t color);
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
};

#endif // DISPLAY_RENDERER_H
