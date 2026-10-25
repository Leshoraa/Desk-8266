#include "DisplayRenderer.h"
#include "font5x7.h"

namespace {
const char* const HOUR_LABELS[12] = {
  "12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"
};

// 7 Settings Items (Camera Rotary Mode Dial closely packed)
const char* const SETTING_NAMES[SETTINGS_NUM_ITEMS] = {
  "BRIGHT", "GLANCE", "NIGHT", "GYRO", "THEME", "INFO", "EXIT"
};

const char SETTING_BADGE_CHARS[SETTINGS_NUM_ITEMS] = {
  'B', 'G', 'N', 'R', 'T', 'i', 'X'
};

const char* const SETTING_TITLES[SETTINGS_NUM_ITEMS] = {
  "BRIGHTNESS", "AUTO GLANCE", "NIGHT STAND", "GYRO TRACKING", "THEME ACCENT", "DEVICE INFO", "EXIT MENU"
};

const char* const SETTING_DESCS[SETTINGS_NUM_ITEMS] = {
  "HOLD: CONFIGURE", "HOLD: TOGGLE", "HOLD: TOGGLE", "HOLD: TOGGLE", "HOLD: CONFIGURE", "HOLD: VIEW INFO", "TAP 2X: EXIT"
};

const char* const BRIGHTNESS_LABELS[BRIGHTNESS_LEVEL_COUNT] = {
  "10%", "40%", "60%", "100%"
};

const uint16_t THEME_COLORS[4] = {
  COLOR_SAGE_GREEN, // 0: Sage Green (#6BAF84)
  0x3DD1,           // 1: Mint Aqua (#3bbb8c)
  COLOR_AMBER,      // 2: Warm Amber (#FDA0)
  COLOR_WHITE       // 3: Crisp Monochrome (#FFFFFF)
};

const char* const THEME_NAMES[4] = {
  "SAGE", "MINT", "AMBER", "WHITE"
};

// Fast Q8 color blender interpolating directly across RGB565 bitfields without floating-point math
inline uint16_t blendColor565(uint16_t c0, uint16_t c1, uint16_t alpha256) {
  if (alpha256 >= 256) return c1;
  if (alpha256 == 0)   return c0;

  uint32_t r0 = (c0 >> 11) & 0x1F;
  uint32_t g0 = (c0 >> 5)  & 0x3F;
  uint32_t b0 =  c0        & 0x1F;

  uint32_t r1 = (c1 >> 11) & 0x1F;
  uint32_t g1 = (c1 >> 5)  & 0x3F;
  uint32_t b1 =  c1        & 0x1F;

  uint32_t r = (r0 * (256 - alpha256) + r1 * alpha256) >> 8;
  uint32_t g = (g0 * (256 - alpha256) + g1 * alpha256) >> 8;
  uint32_t b = (b0 * (256 - alpha256) + b1 * alpha256) >> 8;

  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}
}

DisplayRenderer::DisplayRenderer()
  : tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST),
    lastRenderedPitch(-999.0f),
    lastRenderedTouch(false),
    lastRenderedTemp(-999.0f),
    lastRenderedHum(-999.0f),
    lastRenderedMode(static_cast<AppMode>(-1)),
    lastClockRenderMs(0),
    lastProgressBarWidth(0),
    lastProgressPercent(-1),
    isWiFiStaticDrawn(false),
    clockWeightMorph(1.0f),
    sensorWeightMorph(0.0f),
    settingsWeightMorph(0.0f),
    lastMorphUpdateMs(0),
    currentSettingIndex(0),
    currentWheelAngle(0.0f),
    targetWheelAngle(0.0f),
    isExitRequested(false),
    inSubMenu(false),
    brightnessStepIndex(3), // Default 100%
    infoSubIndex(0),
    userBrightness(FULL_STAND_BRIGHTNESS),
    autoPeekEnabled(true),
    nightStandEnabled(true),
    gyroTrackingEnabled(true),
    themeIndex(0),
    lastSettingTouchMs(0),
    lastWeatherParticleUpdateMs(0),
    lastThunderFlashMs(0),
    isNightStandActive(false),
    currentBacklightLevel(FULL_STAND_BRIGHTNESS) {
  for (int p = 0; p < 8; p++) {
    weatherParticles[p].dist = p * 10.0f;
    weatherParticles[p].speed = 2.4f + (p % 4) * 0.4f;
  }
}

uint16_t DisplayRenderer::getThemeAccentColor() const {
  return THEME_COLORS[themeIndex % 4];
}

void DisplayRenderer::openSettings() {
  lastSettingTouchMs = millis();
  isExitRequested = false;
  inSubMenu = false;
  currentSettingIndex = 0;
  targetWheelAngle = 0.0f;
  currentWheelAngle = 0.0f;
}

void DisplayRenderer::advanceSettings() {
  lastSettingTouchMs = millis();
  currentSettingIndex = (currentSettingIndex + 1) % SETTINGS_NUM_ITEMS;
  targetWheelAngle = static_cast<float>(currentSettingIndex) * SETTINGS_STEP_ANGLE;
}

void DisplayRenderer::enterSubMenu() {
  lastSettingTouchMs = millis();
  if (currentSettingIndex == (SETTINGS_NUM_ITEMS - 1)) {
    // Hold on EXIT returns to Clock
    isExitRequested = true;
    return;
  }
  inSubMenu = true;
}

void DisplayRenderer::advanceSubMenu() {
  lastSettingTouchMs = millis();
  switch (currentSettingIndex) {
    case 0: // BRIGHT: cycle 10% -> 40% -> 60% -> 100%
      brightnessStepIndex = (brightnessStepIndex + 1) % BRIGHTNESS_LEVEL_COUNT;
      userBrightness = BRIGHTNESS_LEVELS[brightnessStepIndex];
      currentBacklightLevel = userBrightness;
      pinMode(PIN_TFT_BL, OUTPUT);
      analogWrite(PIN_TFT_BL, userBrightness);
      break;

    case 1: // GLANCE: toggle ON / OFF
      autoPeekEnabled = !autoPeekEnabled;
      break;

    case 2: // NIGHT: toggle ENABLE / DISABLE
      nightStandEnabled = !nightStandEnabled;
      break;

    case 3: // GYRO: toggle DYNAMIC (tilt follow) / LOCKED (fixed 0 deg)
      gyroTrackingEnabled = !gyroTrackingEnabled;
      break;

    case 4: // THEME: cycle SAGE -> MINT -> AMBER -> WHITE
      themeIndex = (themeIndex + 1) % 4;
      break;

    case 5: // INFO: cycle through diagnostics (IP -> RSSI -> Free Heap -> CPU Clock -> Uptime)
      infoSubIndex = (infoSubIndex + 1) % 6;
      break;

    default:
      break;
  }
}

void DisplayRenderer::exitSubMenu() {
  lastSettingTouchMs = millis();
  inSubMenu = false;
}

void DisplayRenderer::begin() {
  tft.begin(80000000); // 80MHz high-speed SPI clock for GC9A01 (sub-12ms frame transfer)
  tft.setRotation(0);

  // Re-claim PIN_TFT_BL (GPIO 12) from SPI MISO back to dedicated GPIO PWM output
  pinMode(PIN_TFT_BL, OUTPUT);
  analogWriteRange(255);
  analogWriteFreq(1000);
  analogWrite(PIN_TFT_BL, FULL_STAND_BRIGHTNESS);
  currentBacklightLevel = FULL_STAND_BRIGHTNESS;
}

void DisplayRenderer::printFlickerFreeCentered(const char* text, int y, uint8_t size, uint16_t color, int totalChars) {
  if (!text) return;
  int len = strlen(text);
  if (len > totalChars) totalChars = len;

  int totalPad = totalChars - len;
  int leftPad = totalPad / 2;
  int rightPad = totalPad - leftPad;

  char buffer[40];
  snprintf(buffer, sizeof(buffer), "%*s%s%*s", leftPad, "", text, rightPad, "");

  int textPixelWidth = totalChars * 6 * size;
  int x = (SCREEN_WIDTH - textPixelWidth) / 2;
  if (x < 0) x = 0;

  tft.setTextSize(size);
  tft.setTextColor(color, COLOR_BLACK);
  tft.setCursor(x, y);
  tft.print(buffer);
}

void DisplayRenderer::showWiFiProgress(const char* statusText, int percent, const char* detailText) {
  if (!isWiFiStaticDrawn) {
    tft.fillScreen(COLOR_BLACK);
    tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, BEZEL_OUTER_RADIUS, COLOR_WHITE);
    tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, BEZEL_INNER_RADIUS, COLOR_DARKGRAY);

    printFlickerFreeCentered("Connecting", 70, 1, COLOR_WHITE, 18);
    printFlickerFreeCentered(WIFI_SSID, 88, 2, COLOR_WHITE, 12);

    // Centered vertical progress bar bounding outline
    tft.drawRoundRect(50, 115, 140, 10, 5, COLOR_WHITE);
    isWiFiStaticDrawn = true;
    lastProgressBarWidth = 0;
    lastProgressPercent = -1;
  }

  if (statusText) {
    uint16_t statusColor = (percent >= 100) ? COLOR_GREEN : COLOR_WHITE;
    printFlickerFreeCentered(statusText, 70, 1, statusColor, 18);
  }

  if (percent != lastProgressPercent) {
    lastProgressPercent = percent;

    int targetFillWidth = (percent * 134) / 100;
    if (targetFillWidth > 134) targetFillWidth = 134;

    uint16_t barColor = (percent >= 100) ? COLOR_GREEN : COLOR_WHITE;
    if (percent >= 100) {
      tft.fillRoundRect(53, 117, 134, 6, 2, COLOR_GREEN);
    } else if (targetFillWidth > lastProgressBarWidth) {
      tft.fillRoundRect(53 + lastProgressBarWidth, 117, targetFillWidth - lastProgressBarWidth, 6, 2, barColor);
      lastProgressBarWidth = targetFillWidth;
    }

    char percentBuf[16];
    snprintf(percentBuf, sizeof(percentBuf), "%3d%%", percent);
    printFlickerFreeCentered(percentBuf, 138, 1, barColor, 10);
  }

  if (detailText) {
    printFlickerFreeCentered(detailText, 156, 1, COLOR_WHITE, 22);
  }
}

void DisplayRenderer::renderStaticBorder() {
  tft.fillScreen(COLOR_CLOCK_BG);
  tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, BEZEL_OUTER_RADIUS, COLOR_WHITE);
  tft.drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, BEZEL_INNER_RADIUS, COLOR_DARKGRAY);
}

void DisplayRenderer::drawLineToBand(int x0, int y0, int x1, int y1, int yStart, uint16_t color) {
  int yEnd = yStart + BAND_HEIGHT - 1;

  // Bounding box rejection: skip lines completely outside current band
  if ((y0 < yStart && y1 < yStart) || (y0 > yEnd && y1 > yEnd)) return;
  if ((x0 < 0 && x1 < 0) || (x0 >= SCREEN_WIDTH && x1 >= SCREEN_WIDTH)) return;

  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while (true) {
    if ((unsigned)x0 < SCREEN_WIDTH && y0 >= yStart && y0 <= yEnd) {
      screenBand[(y0 - yStart) * SCREEN_WIDTH + x0] = color;
    }
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void DisplayRenderer::drawCenterCapToBand(int cx, int cy, int yStart, uint16_t color, uint16_t holeColor) {
  int yEnd = yStart + BAND_HEIGHT - 1;
  if (cy + CENTER_PIN_RADIUS < yStart || cy - CENTER_PIN_RADIUS > yEnd) return;

  for (int dy = -CENTER_PIN_RADIUS; dy <= CENTER_PIN_RADIUS; dy++) {
    int py = cy + dy;
    if (py >= yStart && py <= yEnd) {
      int rowOffset = (py - yStart) * SCREEN_WIDTH;
      for (int dx = -CENTER_PIN_RADIUS; dx <= CENTER_PIN_RADIUS; dx++) {
        int distSq = dx * dx + dy * dy;
        if (distSq <= (CENTER_PIN_RADIUS * CENTER_PIN_RADIUS)) {
          int px = cx + dx;
          if ((unsigned)px < SCREEN_WIDTH) {
            screenBand[rowOffset + px] = (distSq <= 2) ? holeColor : color;
          }
        }
      }
    }
  }
}

void DisplayRenderer::drawCircleToBand(int cx, int cy, int radius, int yStart, uint16_t color) {
  int yEnd = yStart + BAND_HEIGHT - 1;
  if (cy + radius < yStart || cy - radius > yEnd) return;
  if (cx + radius < 0 || cx - radius >= SCREEN_WIDTH) return;

  int rSq = radius * radius;
  for (int dy = -radius; dy <= radius; dy++) {
    int py = cy + dy;
    if (py >= yStart && py <= yEnd) {
      int rowOffset = (py - yStart) * SCREEN_WIDTH;
      for (int dx = -radius; dx <= radius; dx++) {
        if (dx * dx + dy * dy <= rSq) {
          int px = cx + dx;
          if (static_cast<unsigned>(px) < SCREEN_WIDTH) {
            screenBand[rowOffset + px] = color;
          }
        }
      }
    }
  }
}

void DisplayRenderer::drawCircleOutlineToBand(int cx, int cy, int radius, int yStart, uint16_t color) {
  int yEnd = yStart + BAND_HEIGHT - 1;
  if (cy + radius < yStart || cy - radius > yEnd) return;
  if (cx + radius < 0 || cx - radius >= SCREEN_WIDTH) return;

  auto plot = [this, yStart, yEnd, color](int px, int py) {
    if (py >= yStart && py <= yEnd && static_cast<unsigned>(px) < SCREEN_WIDTH) {
      screenBand[(py - yStart) * SCREEN_WIDTH + px] = color;
    }
  };

  int f = 1 - radius;
  int ddF_x = 1;
  int ddF_y = -2 * radius;
  int x = 0;
  int y = radius;

  plot(cx, cy + radius);
  plot(cx, cy - radius);
  plot(cx + radius, cy);
  plot(cx - radius, cy);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    plot(cx + x, cy + y);
    plot(cx - x, cy + y);
    plot(cx + x, cy - y);
    plot(cx - x, cy - y);
    plot(cx + y, cy + x);
    plot(cx - y, cy + x);
    plot(cx + y, cy - x);
    plot(cx - y, cy - x);
  }
}

void DisplayRenderer::drawCapsuleToBand(int cx, int cy, int hw, int hh, float angleRad, int yStart, uint16_t color) {
  int yEnd = yStart + BAND_HEIGHT - 1;
  int maxDim = hw + hh + 2;
  if (cy + maxDim < yStart || cy - maxDim > yEnd) return;
  if (cx + maxDim < 0 || cx - maxDim >= SCREEN_WIDTH) return;

  int32_t ca = static_cast<int32_t>(cosf(angleRad) * 1024.0f);
  int32_t sa = static_cast<int32_t>(sinf(angleRad) * 1024.0f);

  int straightLen = hw - hh;
  static const uint8_t CAP_DX_R8[9] = { 8, 8, 8, 7, 7, 6, 5, 4, 0 };

  // Fill solid pill capsule scanline by scanline
  for (int dy = -hh; dy <= hh; dy++) {
    int absDy = abs(dy);
    int capDx = 0;
    if (hh == 8 && absDy <= 8) {
      capDx = CAP_DX_R8[absDy];
    } else {
      int val = hh * hh - dy * dy;
      if (val > 0) capDx = static_cast<int>(roundf(sqrtf(static_cast<float>(val))));
    }

    int minLx = -straightLen - capDx;
    int maxLx = straightLen + capDx;

    for (int lx = minLx; lx <= maxLx; lx++) {
      int px = cx + static_cast<int>((lx * ca - dy * sa + 512) >> 10);
      int py = cy + static_cast<int>((lx * sa + dy * ca + 512) >> 10);
      if (static_cast<unsigned>(px) < SCREEN_WIDTH && py >= yStart && py <= yEnd) {
        screenBand[(py - yStart) * SCREEN_WIDTH + px] = color;
      }
    }
  }
}

void DisplayRenderer::drawRotatedStringToBand(int cx, int cy, const char* str, float angleRad, uint8_t size, int yStart, uint16_t color) {
  if (!str || !str[0]) return;
  int yEnd = yStart + BAND_HEIGHT - 1;
  int len = strlen(str);

  int totalWidth = len * 6 * static_cast<int>(size) - static_cast<int>(size);
  int totalHeight = 7 * static_cast<int>(size);

  int maxDim = (totalWidth > totalHeight ? totalWidth : totalHeight);
  if (cy + maxDim < yStart || cy - maxDim > yEnd) return;
  if (cx + maxDim < 0 || cx - maxDim >= SCREEN_WIDTH) return;

  int startX = -(totalWidth >> 1);
  int startY = -(totalHeight >> 1);

  // Q10 fixed-point arithmetic avoids floating point division on pixel hot-paths
  int32_t cosVal = static_cast<int32_t>(cosf(angleRad) * 1024.0f);
  int32_t sinVal = static_cast<int32_t>(sinf(angleRad) * 1024.0f);
  int charAdvance = 6 * static_cast<int>(size);

  for (int i = 0; i < len; i++) {
    char ch = str[i];
    if (ch < 32 || ch > 126) continue;
    uint16_t fontOffset = (ch - 32) * 5;
    int charLeft = startX + i * charAdvance;

    for (int col = 0; col < 5; col++) {
      uint8_t columnBits = pgm_read_byte(&font5x7[fontOffset + col]);
      if (!columnBits) continue;

      for (int row = 0; row < 7; row++) {
        if (!(columnBits & (1 << row))) continue;

        for (uint8_t sx = 0; sx < size; sx++) {
          int localX = charLeft + col * static_cast<int>(size) + sx;
          int32_t rotX = static_cast<int32_t>(localX) * cosVal;
          int32_t rotY = static_cast<int32_t>(localX) * sinVal;

          for (uint8_t sy = 0; sy < size; sy++) {
            int localY = startY + row * static_cast<int>(size) + sy;

            int px = cx + static_cast<int>((rotX - static_cast<int32_t>(localY) * sinVal + 512) >> 10);
            int py = cy + static_cast<int>((rotY + static_cast<int32_t>(localY) * cosVal + 512) >> 10);

            if ((unsigned)px < SCREEN_WIDTH && py >= yStart && py <= yEnd) {
              screenBand[(py - yStart) * SCREEN_WIDTH + px] = color;
            }
          }
        }
      }
    }
  }
}

void DisplayRenderer::drawRotatedScaledStringToBand(int cx, int cy, const char* str, float angleRad, float scale, int yStart, uint16_t color) {
  if (!str || !str[0] || scale <= 0.05f) return;
  int yEnd = yStart + BAND_HEIGHT - 1;
  int len = strlen(str);

  float totalWidth = (len * 6.0f - 1.0f) * scale;
  float totalHeight = 7.0f * scale;

  float maxDim = (totalWidth > totalHeight ? totalWidth : totalHeight);
  if (cy + maxDim < yStart || cy - maxDim > yEnd) return;
  if (cx + maxDim < 0 || cx - maxDim >= SCREEN_WIDTH) return;

  float startX = -0.5f * totalWidth;
  float startY = -0.5f * totalHeight;

  int32_t cosVal = static_cast<int32_t>(cosf(angleRad) * 1024.0f);
  int32_t sinVal = static_cast<int32_t>(sinf(angleRad) * 1024.0f);

  for (int i = 0; i < len; i++) {
    char ch = str[i];
    if (ch < 32 || ch > 126) continue;
    uint16_t fontOffset = (ch - 32) * 5;
    float charLeft = startX + (i * 6.0f * scale);

    for (int col = 0; col < 5; col++) {
      uint8_t columnBits = pgm_read_byte(&font5x7[fontOffset + col]);
      if (!columnBits) continue;

      float colLeft = charLeft + (col * scale);
      int xMin = static_cast<int>(roundf(colLeft));
      int xMax = static_cast<int>(roundf(colLeft + scale));
      if (xMax <= xMin) xMax = xMin + 1;

      for (int row = 0; row < 7; row++) {
        if (!(columnBits & (1 << row))) continue;

        float rowTop = startY + (row * scale);
        int yMin = static_cast<int>(roundf(rowTop));
        int yMax = static_cast<int>(roundf(rowTop + scale));
        if (yMax <= yMin) yMax = yMin + 1;

        for (int lx = xMin; lx < xMax; lx++) {
          int32_t rotX = static_cast<int32_t>(lx) * cosVal;
          int32_t rotY = static_cast<int32_t>(lx) * sinVal;

          for (int ly = yMin; ly < yMax; ly++) {
            int px = cx + static_cast<int>((rotX - static_cast<int32_t>(ly) * sinVal + 512) >> 10);
            int py = cy + static_cast<int>((rotY + static_cast<int32_t>(ly) * cosVal + 512) >> 10);

            if (static_cast<unsigned>(px) < SCREEN_WIDTH && py >= yStart && py <= yEnd) {
              screenBand[(py - yStart) * SCREEN_WIDTH + px] = color;
            }
          }
        }
      }
    }
  }
}

void DisplayRenderer::drawWeatherIconToBand(int cx, int cy, WeatherType type, float angleRad, float scale, int yStart, uint16_t color) {
  if (type == WeatherType::Unknown || scale <= 0.05f) return;

  const uint16_t* bmp = nullptr;
  switch (type) {
    case WeatherType::Sun:     bmp = WeatherBitmaps::ICON_SUN; break;
    case WeatherType::Moon:    bmp = WeatherBitmaps::ICON_MOON; break;
    case WeatherType::Cloudy:  bmp = WeatherBitmaps::ICON_CLOUDY; break;
    case WeatherType::Rain:    bmp = WeatherBitmaps::ICON_RAIN; break;
    case WeatherType::Thunder: bmp = WeatherBitmaps::ICON_THUNDER; break;
    default: return;
  }

  int yEnd = yStart + BAND_HEIGHT - 1;
  float maxDim = 12.0f * scale;
  if (cy + maxDim < yStart || cy - maxDim > yEnd) return;
  if (cx + maxDim < 0 || cx - maxDim >= SCREEN_WIDTH) return;

  int32_t ca = static_cast<int32_t>(cosf(angleRad) * 1024.0f);
  int32_t sa = static_cast<int32_t>(sinf(angleRad) * 1024.0f);

  for (int r = 0; r < 16; r++) {
    uint16_t rowBits = pgm_read_word(&bmp[r]);
    if (!rowBits) continue;

    float rTop = (r - 8.0f) * scale;
    int yMin = static_cast<int>(roundf(rTop));
    int yMax = static_cast<int>(roundf(rTop + scale));
    if (yMax <= yMin) yMax = yMin + 1;

    for (int c = 0; c < 16; c++) {
      if (!(rowBits & (1 << (15 - c)))) continue;

      float cLeft = (c - 8.0f) * scale;
      int xMin = static_cast<int>(roundf(cLeft));
      int xMax = static_cast<int>(roundf(cLeft + scale));
      if (xMax <= xMin) xMax = xMin + 1;

      for (int lx = xMin; lx < xMax; lx++) {
        int32_t rotX = static_cast<int32_t>(lx) * ca;
        int32_t rotY = static_cast<int32_t>(lx) * sa;

        for (int ly = yMin; ly < yMax; ly++) {
          int px = cx + static_cast<int>((rotX - static_cast<int32_t>(ly) * sa + 512) >> 10);
          int py = cy + static_cast<int>((rotY + static_cast<int32_t>(ly) * ca + 512) >> 10);

          if (static_cast<unsigned>(px) < SCREEN_WIDTH && py >= yStart && py <= yEnd) {
            screenBand[(py - yStart) * SCREEN_WIDTH + px] = color;
          }
        }
      }
    }
  }
}

void DisplayRenderer::renderFrame(float pitchDeg, bool isTouched, float temperature, float humidity,
                                  int hour, int minute, int second, unsigned long subSecondMs,
                                  const char* dateText, WeatherType weatherType, AppMode mode,
                                  const char* wifiIp, int wifiRssi) {
  unsigned long nowMs = millis();
  float pitchRad = pitchDeg * (3.14159265f / 180.0f);

  float renderDelta = fabsf(pitchDeg - lastRenderedPitch);
  if (renderDelta > 180.0f) renderDelta = fabsf(renderDelta - 360.0f);

  // 1. Continuous Reversible 3-Way Mode Morphing State Machine
  float targetClock    = (mode == AppMode::Clock) ? 1.0f : 0.0f;
  float targetSensor   = (mode == AppMode::Sensors) ? 1.0f : 0.0f;
  float targetSettings = (mode == AppMode::Settings) ? 1.0f : 0.0f;

  if (lastMorphUpdateMs == 0) {
    lastMorphUpdateMs = nowMs;
    clockWeightMorph    = targetClock;
    sensorWeightMorph   = targetSensor;
    settingsWeightMorph = targetSettings;
  }
  float dt = static_cast<float>(nowMs - lastMorphUpdateMs);
  lastMorphUpdateMs = nowMs;
  if (dt > 100.0f) dt = 16.67f; // Guard against initial delay or paused frame

  float step = dt / static_cast<float>(MODE_TRANSITION_MS);
  auto updateWeight = [step](float& cur, float tgt) {
    if (cur < tgt) { cur += step; if (cur > tgt) cur = tgt; }
    else if (cur > tgt) { cur -= step; if (cur < tgt) cur = tgt; }
  };
  updateWeight(clockWeightMorph, targetClock);
  updateWeight(sensorWeightMorph, targetSensor);
  updateWeight(settingsWeightMorph, targetSettings);

  // Hermite cubic ease-in-out curve ensures silky smooth reversal and zero jerk
  float clockWeight    = clockWeightMorph * clockWeightMorph * (3.0f - 2.0f * clockWeightMorph);
  float sensorWeight   = sensorWeightMorph * sensorWeightMorph * (3.0f - 2.0f * sensorWeightMorph);
  float settingsWeight = settingsWeightMorph * settingsWeightMorph * (3.0f - 2.0f * settingsWeightMorph);

  // Settings Wheel Smooth Rotary Easing
  float angleDiff = targetWheelAngle - currentWheelAngle;
  if (fabsf(angleDiff) > 0.0005f) {
    currentWheelAngle += angleDiff * 0.22f; // Smooth exponential ease-out
  } else {
    currentWheelAngle = targetWheelAngle;
  }

  lastClockRenderMs = nowMs;
  lastRenderedPitch = pitchDeg;
  lastRenderedTouch = isTouched;
  lastRenderedTemp  = temperature;
  lastRenderedHum   = humidity;
  lastRenderedMode  = mode;

  // 2. Flip-to-Stealth Night Stand Mode (Upside down flip triggers gentle amber dimming)
  bool targetNight = nightStandEnabled && (pitchDeg >= NIGHT_STAND_PITCH_MIN && pitchDeg <= NIGHT_STAND_PITCH_MAX);
  if (targetNight != isNightStandActive) {
    isNightStandActive = targetNight;
  }
  uint8_t targetBL = isNightStandActive ? NIGHT_STAND_BRIGHTNESS : userBrightness;
  if (currentBacklightLevel != targetBL) {
    if (currentBacklightLevel < targetBL) {
      currentBacklightLevel = (targetBL - currentBacklightLevel > 15) ? (currentBacklightLevel + 15) : targetBL;
    } else {
      currentBacklightLevel = (currentBacklightLevel - targetBL > 15) ? (currentBacklightLevel - 15) : targetBL;
    }
    pinMode(PIN_TFT_BL, OUTPUT);
    analogWrite(PIN_TFT_BL, currentBacklightLevel);
  }

  uint16_t activeBgColor    = isNightStandActive ? COLOR_NIGHT_BG : COLOR_CLOCK_BG;
  uint16_t primaryColor     = isNightStandActive ? COLOR_NIGHT_AMBER : COLOR_WHITE;
  uint16_t hourMinHandColor = isNightStandActive ? COLOR_NIGHT_AMBER : COLOR_HAND_HOUR_MIN;
  uint16_t secondHandColor  = isNightStandActive ? COLOR_NIGHT_AMBER : COLOR_HAND_SECOND;

  // 3. 3D Parallax Crystal Glass Layering:
  // Hands float slightly forward towards the viewer (+1.8px along in-plane tilt normal)
  float sa_norm = sinf(pitchRad);
  float ca_norm = cosf(pitchRad);
  float parallaxHandsX = 1.8f * sa_norm;
  float parallaxHandsY = -1.8f * ca_norm;

  // Discrete 1-second hand positions (traditional quartz clock ticking)
  float sValHand = static_cast<float>(second);
  float mValHand = static_cast<float>(minute) + (sValHand / 60.0f);
  float hValHand = static_cast<float>(hour % 12) + (mValHand / 60.0f);

  float hAngle = (hValHand * (3.14159265f / 6.0f))  - 1.5707963f + pitchRad;
  float mAngle = (mValHand * (3.14159265f / 30.0f)) - 1.5707963f + pitchRad;
  float sAngle = (sValHand * (3.14159265f / 30.0f)) - 1.5707963f + pitchRad;

  // Hand direction and perpendicular offset vectors
  float ca_h = cosf(hAngle), sa_h = sinf(hAngle);
  float px_h = -sa_h, py_h = ca_h;

  float ca_m = cosf(mAngle), sa_m = sinf(mAngle);
  float px_m = -sa_m, py_m = ca_m;

  float ca_s = cosf(sAngle), sa_s = sinf(sAngle);

  // Active Digits: Invisible second hand dynamic magnification wave combined with steady hour & minute hand numbers
  struct ActiveDigit {
    uint8_t index;
    int16_t nx, ny;
    float scale;
    uint16_t numColor;
  };
  ActiveDigit activeDigits[8];
  int activeDigitCount = 0;

  if (clockWeight > 0.01f) {
    // Continuous hour hand dial position [0.0 .. 12.0)
    float hVal = static_cast<float>(hour % 12) + (static_cast<float>(minute) / 60.0f) + (static_cast<float>(second) / 3600.0f);
    while (hVal >= 12.0f) hVal -= 12.0f;
    while (hVal < 0.0f)   hVal += 12.0f;

    // Continuous minute hand dial position [0.0 .. 12.0)
    float mVal = (static_cast<float>(minute) / 5.0f) + (static_cast<float>(second) / 300.0f);
    while (mVal >= 12.0f) mVal -= 12.0f;
    while (mVal < 0.0f)   mVal += 12.0f;

    // Continuous invisible second hand position driving expansion/contraction wave [0.0 .. 12.0)
    float sValAnim = static_cast<float>(second) + (static_cast<float>(subSecondMs) / 1000.0f);
    float sPos = sValAnim / 5.0f;
    while (sPos >= 12.0f) sPos -= 12.0f;
    while (sPos < 0.0f)   sPos += 12.0f;

    uint16_t targetColor = isNightStandActive ? COLOR_NIGHT_AMBER : COLOR_WHITE;

    for (int idx = 0; idx < 12; idx++) {
      float dH = fabsf(static_cast<float>(idx) - hVal);
      if (dH > 6.0f) dH = 12.0f - dH;

      float dM = fabsf(static_cast<float>(idx) - mVal);
      if (dM > 6.0f) dM = 12.0f - dM;

      float dS = fabsf(static_cast<float>(idx) - sPos);
      if (dS > 6.0f) dS = 12.0f - dS;

      float factorH = (dH < 1.0f) ? (0.5f * (1.0f + cosf(dH * 3.14159265f))) : 0.0f;
      float factorM = (dM < 1.0f) ? (0.5f * (1.0f + cosf(dM * 3.14159265f))) : 0.0f;
      float factorS = (dS < 1.0f) ? (0.5f * (1.0f + cosf(dS * 3.14159265f))) : 0.0f;

      // Steady visibility for numbers at the hour and minute hands:
      float handFactor = (factorH > factorM) ? factorH : factorM;

      // Combined factor: numbers expand and contract based on direction of invisible second hand,
      // while hour & minute hand numbers stay steadily visible
      float mFactor = (handFactor > factorS) ? handFactor : factorS;
      if (mFactor < 0.02f) continue; // Inactive numbers completely hidden

      // Smooth cubic Hermite curve ensures seamless magnification & bloom
      float fade = mFactor * mFactor * (3.0f - 2.0f * mFactor);
      if (fade > 1.0f) fade = 1.0f;
      if (fade < 0.0f) fade = 0.0f;

      uint16_t alpha256 = static_cast<uint16_t>(roundf(fade * 256.0f * clockWeight));
      if (alpha256 > 256) alpha256 = 256;
      if (alpha256 < 2) continue;

      float numAngle = (idx * (3.14159265f / 6.0f)) - 1.5707963f + pitchRad;

      // Continuous fractional scaling: smooth magnification from compact base to refined peak scale
      float scale = (HOUR_NUMBER_SCALE_BASE + (mFactor * (HOUR_NUMBER_SCALE_PEAK - HOUR_NUMBER_SCALE_BASE))) * (0.4f + 0.6f * clockWeight);

      // Radial glide: lifts smoothly from 84px to 89px
      float rNum = HOUR_NUMBER_BASE_R + (mFactor * HOUR_NUMBER_LIFT_R);
      int nx = static_cast<int>(roundf(120.0f + rNum * cosf(numAngle)));
      int ny = static_cast<int>(roundf(120.0f + rNum * sinf(numAngle)));

      uint16_t numColor = blendColor565(activeBgColor, targetColor, alpha256);

      if (activeDigitCount < 8) {
        activeDigits[activeDigitCount++] = {
          static_cast<uint8_t>(idx),
          static_cast<int16_t>(nx),
          static_cast<int16_t>(ny),
          scale,
          numColor
        };
      }
    }
  }

  struct SettingDrawItem {
    int16_t x, y;
    float relAngle;
    uint8_t index;
    bool isApex;
  };
  SettingDrawItem settingDrawList[SETTINGS_NUM_ITEMS];
  int visibleSettingCount = 0;

  if (settingsWeight > 0.01f) {
    for (int k = 0; k < SETTINGS_NUM_ITEMS; k++) {
      float relA = (static_cast<float>(k) * SETTINGS_STEP_ANGLE) - currentWheelAngle;
      while (relA > 3.14159265f)  relA -= 6.2831853f;
      while (relA < -3.14159265f) relA += 6.2831853f;

      // Visible in the upper arc (-95 deg to +95 deg)
      if (fabsf(relA) < 1.65f) {
        bool isApex = (fabsf(relA) < (SETTINGS_STEP_ANGLE * 0.5f));
        float r = isApex ? SETTINGS_APEX_RADIUS : SETTINGS_TRACK_RADIUS;
        float scrA = relA - 1.5707963f + pitchRad;
        int sx = 120 + static_cast<int>(roundf(r * cosf(scrA)));
        int sy = 120 + static_cast<int>(roundf(r * sinf(scrA)));
        settingDrawList[visibleSettingCount++] = {
          static_cast<int16_t>(sx), static_cast<int16_t>(sy), relA, static_cast<uint8_t>(k), isApex
        };
      }
    }
  }

  int16_t apexX1[2], apexY1[2], apexX2[2], apexY2[2];
  int16_t pagerDotX[SETTINGS_NUM_ITEMS];
  int16_t pagerDotY[SETTINGS_NUM_ITEMS];

  if (settingsWeight > 0.01f) {
    // Precompute knurled hash marks: 61 ticks spanning -90 deg to +90 deg upper arc, short & dense
    for (int t = -30; t <= 30; t++) {
      float a = (static_cast<float>(t) * 0.052f) - 1.5707963f + pitchRad;
      float ca = cosf(a), sa = sinf(a);
      bool isMajor = (t % 5 == 0);
      float rIn = isMajor ? 109.0f : 112.0f;
      int idx = t + 30;
      knurlTicks[idx].x1 = 120 + static_cast<int16_t>(roundf(rIn * ca));
      knurlTicks[idx].y1 = 120 + static_cast<int16_t>(roundf(rIn * sa));
      knurlTicks[idx].x2 = 120 + static_cast<int16_t>(roundf(115.0f * ca));
      knurlTicks[idx].y2 = 120 + static_cast<int16_t>(roundf(115.0f * sa));
      knurlTicks[idx].isMajor = isMajor;
    }

    // Precompute apex pointer line (12 o'clock vertical mark pointing to capsule, 2px bold)
    float apexA = -1.5707963f + pitchRad;
    float caApex = cosf(apexA), saApex = sinf(apexA);
    float pxApex = -saApex, pyApex = caApex;
    for (int off = 0; off <= 1; off++) {
      float o = (off == 0) ? -0.5f : 0.5f;
      apexX1[off] = 120 + static_cast<int16_t>(roundf(96.0f * caApex + o * pxApex));
      apexY1[off] = 120 + static_cast<int16_t>(roundf(96.0f * saApex + o * pyApex));
      apexX2[off] = 120 + static_cast<int16_t>(roundf(115.0f * caApex + o * pxApex));
      apexY2[off] = 120 + static_cast<int16_t>(roundf(115.0f * saApex + o * pyApex));
    }

    // Precompute pager dots (7 items: center is index 3)
    int cxMid = 120 + static_cast<int>(roundf(10.0f * sa_norm));
    int cyPager = (120 - static_cast<int>(roundf(10.0f * ca_norm))) + 36;
    for (int d = 0; d < SETTINGS_NUM_ITEMS; d++) {
      float dotOffset = (static_cast<float>(d) - 3.0f) * 9.0f;
      pagerDotX[d] = cxMid + static_cast<int16_t>(roundf(dotOffset * ca_norm));
      pagerDotY[d] = cyPager + static_cast<int16_t>(roundf(dotOffset * sa_norm));
    }
  }

  char tempString[12];
  char humString[16];
  constexpr int NUM_GAUGE_STEPS = 50;
  int16_t tempGaugeX[NUM_GAUGE_STEPS][3];
  int16_t tempGaugeY[NUM_GAUGE_STEPS][3];
  int16_t humGaugeX[NUM_GAUGE_STEPS][3];
  int16_t humGaugeY[NUM_GAUGE_STEPS][3];
  int tempFillCount = 0;
  int humFillCount = 0;

  if (sensorWeight > 0.01f) {
    if (!isnan(temperature)) {
      snprintf(tempString, sizeof(tempString), "%.1f ^C", temperature);
    } else {
      snprintf(tempString, sizeof(tempString), "--.- ^C");
    }
    if (!isnan(humidity)) {
      snprintf(humString, sizeof(humString), "HUM: %.0f%%", humidity);
    } else {
      snprintf(humString, sizeof(humString), "HUM: --%");
    }

    // Precalculate circular arc gauge points once per frame across 3 concentric radii
    float tClamped = !isnan(temperature) ? temperature : 25.0f;
    if (tClamped < TEMP_GAUGE_MIN) tClamped = TEMP_GAUGE_MIN;
    if (tClamped > TEMP_GAUGE_MAX) tClamped = TEMP_GAUGE_MAX;
    float tRatio = (tClamped - TEMP_GAUGE_MIN) / (TEMP_GAUGE_MAX - TEMP_GAUGE_MIN);
    tempFillCount = static_cast<int>(roundf(tRatio * static_cast<float>(NUM_GAUGE_STEPS)));

    float hClamped = !isnan(humidity) ? humidity : 50.0f;
    if (hClamped < HUM_GAUGE_MIN) hClamped = HUM_GAUGE_MIN;
    if (hClamped > HUM_GAUGE_MAX) hClamped = HUM_GAUGE_MAX;
    float hRatio = (hClamped - HUM_GAUGE_MIN) / (HUM_GAUGE_MAX - HUM_GAUGE_MIN);
    humFillCount = static_cast<int>(roundf(hRatio * static_cast<float>(NUM_GAUGE_STEPS)));

    float tempMinRad = 2.356194f + pitchRad;
    float step = 1.570796f / static_cast<float>(NUM_GAUGE_STEPS - 1);

    for (int i = 0; i < NUM_GAUGE_STEPS; i++) {
      float a = tempMinRad + static_cast<float>(i) * step;
      float ca = cosf(a);
      float sa = sinf(a);
      for (int t = 0; t < 3; t++) {
        float r = 96.0f - static_cast<float>(t);
        tempGaugeX[i][t] = static_cast<int16_t>(roundf(120.0f + r * ca));
        tempGaugeY[i][t] = static_cast<int16_t>(roundf(120.0f + r * sa));
      }
    }

    float humMinRad = 0.785398f + pitchRad;
    for (int i = 0; i < NUM_GAUGE_STEPS; i++) {
      float a = humMinRad - static_cast<float>(i) * step;
      float ca = cosf(a);
      float sa = sinf(a);
      for (int t = 0; t < 3; t++) {
        float r = 96.0f - static_cast<float>(t);
        humGaugeX[i][t] = static_cast<int16_t>(roundf(120.0f + r * ca));
        humGaugeY[i][t] = static_cast<int16_t>(roundf(120.0f + r * sa));
      }
    }
  }

  // 4. Precalculate Mode 2 Dynamic Scale In-Out & Opacity
  float iconScale = 1.50f * (0.25f + 0.75f * sensorWeight);
  float tempScale = 2.80f * (0.25f + 0.75f * sensorWeight);
  float humScale  = 1.50f * (0.25f + 0.75f * sensorWeight);
  uint16_t sensorAlpha = static_cast<uint16_t>(roundf(sensorWeight * 255.0f));

  // 5. Dynamic Weather Ambiance: Rigidly attached to dial frame & rotating synchronously
  struct WeatherDrawParticle {
    int16_t x0, y0, x1, y1;
    uint16_t color;
  };
  WeatherDrawParticle weatherDrawList[8];
  int weatherDrawCount = 0;

  if (sensorWeight > 0.05f) {
    if (weatherType == WeatherType::Rain || weatherType == WeatherType::Thunder) {
      // Calculate cloud icon center position in current gyro reference frame
      float weatherAngle = -1.5707963f + pitchRad;
      float cxWeather = 120.0f + WEATHER_ICON_RADIUS * cosf(weatherAngle);
      float cyWeather = 120.0f + WEATHER_ICON_RADIUS * sinf(weatherAngle);

      // In dial frame: direction from cloud downwards towards dial center & 6 o'clock
      float downX = -cosf(weatherAngle);
      float downY = -sinf(weatherAngle);
      float crossX = -downY;
      float crossY = downX;

      // Cloud bottom base position rotating synchronously with cloud icon
      float cloudBottomDist = 8.0f * iconScale;
      float baseX = cxWeather + downX * cloudBottomDist;
      float baseY = cyWeather + downY * cloudBottomDist;

      float maxDist = 95.0f * (iconScale / 1.50f);
      float streakLen = 6.0f * (iconScale / 1.50f);

      for (int p = 0; p < 8; p++) {
        weatherParticles[p].dist += weatherParticles[p].speed;
        if (weatherParticles[p].dist > maxDist) {
          weatherParticles[p].dist = 0.0f;
        }

        float spread = (p - 3.5f) * (2.9f * (iconScale / 1.50f)); // Lateral spread across enlarged cloud
        float dropX = baseX + crossX * spread + downX * weatherParticles[p].dist;
        float dropY = baseY + crossY * spread + downY * weatherParticles[p].dist;

        float tailX = dropX + downX * streakLen;
        float tailY = dropY + downY * streakLen;

        int x0 = static_cast<int>(roundf(dropX));
        int y0 = static_cast<int>(roundf(dropY));
        int x1 = static_cast<int>(roundf(tailX));
        int y1 = static_cast<int>(roundf(tailY));

        if (x0 >= 0 && x0 < SCREEN_WIDTH && y0 >= 0 && y0 < SCREEN_HEIGHT &&
            x1 >= 0 && x1 < SCREEN_WIDTH && y1 >= 0 && y1 < SCREEN_HEIGHT) {
          weatherDrawList[weatherDrawCount++] = {
            static_cast<int16_t>(x0), static_cast<int16_t>(y0),
            static_cast<int16_t>(x1), static_cast<int16_t>(y1),
            COLOR_WEATHER_RAIN
          };
        }
      }
    } else if (weatherType == WeatherType::Sun || weatherType == WeatherType::Moon) {
      float sunMoteAngle = (nowMs * 0.002f);
      uint16_t moteCol = (weatherType == WeatherType::Sun) ? COLOR_WEATHER_SUN : COLOR_WEATHER_MOON;
      for (int p = 0; p < 4; p++) {
        float a = sunMoteAngle + (p * 1.570796f);
        float weatherAngle = -1.5707963f + pitchRad;
        int sx = 120 + static_cast<int>(roundf(WEATHER_ICON_RADIUS * cosf(weatherAngle) + (10.0f + p * 2.5f) * cosf(a)));
        int sy = 120 + static_cast<int>(roundf(WEATHER_ICON_RADIUS * sinf(weatherAngle) + (10.0f + p * 2.5f) * sinf(a)));
        weatherDrawList[weatherDrawCount++] = { static_cast<int16_t>(sx), static_cast<int16_t>(sy),
                                               static_cast<int16_t>(sx), static_cast<int16_t>(sy),
                                               moteCol };
      }
    }
  }

  // Render 6 horizontal bands sequentially (240x240 screen, 19.2KB RAM, 100% flicker-free)
  for (int band = 0; band < NUM_BANDS; band++) {
    int yStart = band * BAND_HEIGHT;
    int yEnd   = yStart + BAND_HEIGHT - 1;

    // Fill band buffer with pure black background color
    uint16_t bgColor = activeBgColor;
    uint32_t bg32 = (static_cast<uint32_t>(bgColor) << 16) | bgColor;
    uint32_t* p32 = reinterpret_cast<uint32_t*>(screenBand);
    for (int i = 0; i < (SCREEN_WIDTH * BAND_HEIGHT) / 2; i++) {
      p32[i] = bg32;
    }


    // 1. Mode 1 (Clock Elements) rendered according to clockWeight
    if (clockWeight > 0.01f) {
      uint16_t tickAlpha = static_cast<uint16_t>(roundf(clockWeight * 255.0f));

      // 1. Dual-circle outer bezel imported from WiFi connecting screen
      uint16_t bOuter = isNightStandActive ? COLOR_NIGHT_AMBER : COLOR_WHITE;
      uint16_t bInner = isNightStandActive ? COLOR_NIGHT_BG : COLOR_DARKGRAY;
      uint16_t bOuterA = blendColor565(bgColor, bOuter, tickAlpha);
      uint16_t bInnerA = blendColor565(bgColor, bInner, tickAlpha);
      drawCircleOutlineToBand(SCREEN_CENTER_X, SCREEN_CENTER_Y, BEZEL_OUTER_RADIUS, yStart, bOuterA);
      drawCircleOutlineToBand(SCREEN_CENTER_X, SCREEN_CENTER_Y, BEZEL_INNER_RADIUS, yStart, bInnerA);

      // 2. Active Hour & Minute Digits: Emerges dynamically where hour & minute hands point (independent individual animation)
      for (int i = 0; i < activeDigitCount; i++) {
        drawRotatedScaledStringToBand(activeDigits[i].nx, activeDigits[i].ny,
                                      HOUR_LABELS[activeDigits[i].index],
                                      pitchRad, activeDigits[i].scale, yStart,
                                      activeDigits[i].numColor);
      }

      // 3. Analog Clock Hands (Scaled by clockWeight during morph, centered with 3D Parallax offset)
      float effMinLen  = MINUTE_HAND_LENGTH * clockWeight;
      float effHourLen = HOUR_HAND_LENGTH * clockWeight;
      float effSecLen  = SECOND_HAND_LENGTH * clockWeight;
      float effTailLen = SECOND_HAND_TAIL_LEN * clockWeight;

      float hx0 = 120.0f + parallaxHandsX;
      float hy0 = 120.0f + parallaxHandsY;

      // Hour Hand: 5px width with convex rounded pill tip
      if (effHourLen > 4.0f) {
        for (int k = -2; k <= 2; k++) {
          float off = static_cast<float>(k) * 0.95f;
          float len = (k == 0) ? effHourLen : ((abs(k) == 1) ? (effHourLen - 0.9f) : (effHourLen - 2.2f));
          int x0 = static_cast<int>(roundf(hx0 + off * px_h));
          int y0 = static_cast<int>(roundf(hy0 + off * py_h));
          int x1 = static_cast<int>(roundf(hx0 + len * ca_h + off * px_h));
          int y1 = static_cast<int>(roundf(hy0 + len * sa_h + off * py_h));
          drawLineToBand(x0, y0, x1, y1, yStart, hourMinHandColor);
        }
        int xTip = static_cast<int>(roundf(hx0 + (effHourLen + 0.5f) * ca_h));
        int yTip = static_cast<int>(roundf(hy0 + (effHourLen + 0.5f) * sa_h));
        drawLineToBand(xTip, yTip, xTip, yTip, yStart, hourMinHandColor);
      }

      // Minute Hand: 3px solid baton/pill hand with rounded tip
      if (effMinLen > 5.0f) {
        for (int k = -1; k <= 1; k++) {
          float off = static_cast<float>(k) * 1.0f;
          float len = (k == 0) ? effMinLen : (effMinLen - 1.2f);
          int x0 = static_cast<int>(roundf(hx0 + off * px_m));
          int y0 = static_cast<int>(roundf(hy0 + off * py_m));
          int x1 = static_cast<int>(roundf(hx0 + len * ca_m + off * px_m));
          int y1 = static_cast<int>(roundf(hy0 + len * sa_m + off * py_m));
          drawLineToBand(x0, y0, x1, y1, yStart, hourMinHandColor);
        }
        int xTip = static_cast<int>(roundf(hx0 + effMinLen * ca_m));
        int yTip = static_cast<int>(roundf(hy0 + effMinLen * sa_m));
        drawLineToBand(xTip, yTip, xTip, yTip, yStart, hourMinHandColor);
      }

      // Center Pin Cap (Anchored with 3D parallax, second hand removed per design)
      drawCenterCapToBand(static_cast<int>(roundf(hx0)), static_cast<int>(roundf(hy0)), yStart, hourMinHandColor, bgColor);
      (void)dateText; // Date complication removed for clean minimalist watch face aesthetics
    }

    // 3. Mode 2 (Sensor Elements) rendered according to sensorWeight
    if (sensorWeight > 0.01f) {
      int cxTemp = 120 + static_cast<int>(roundf(10.0f * sa_norm));
      int cyTemp = 120 - static_cast<int>(roundf(10.0f * ca_norm));
      int cxHum  = 120 - static_cast<int>(roundf(24.0f * sa_norm));
      int cyHum  = 120 + static_cast<int>(roundf(24.0f * ca_norm));

      uint16_t activeAccent = isNightStandActive ? COLOR_NIGHT_AMBER : getThemeAccentColor();
      uint16_t activeTempColor = blendColor565(bgColor, primaryColor, sensorAlpha);
      uint16_t activeHumColor  = blendColor565(bgColor, activeAccent, sensorAlpha);

      drawRotatedScaledStringToBand(cxTemp, cyTemp, tempString, pitchRad, tempScale, yStart, activeTempColor);
      drawRotatedScaledStringToBand(cxHum,  cyHum,  humString,  pitchRad, humScale,  yStart, activeHumColor);

      // Weather Complication Icon in Mode 2 above temperature readout (at 12 o'clock position, R=48px)
      if (weatherType != WeatherType::Unknown) {
        float weatherAngle = -1.5707963f + pitchRad;
        int cxWeather = 120 + static_cast<int>(roundf(WEATHER_ICON_RADIUS * cosf(weatherAngle)));
        int cyWeather = 120 + static_cast<int>(roundf(WEATHER_ICON_RADIUS * sinf(weatherAngle)));

        uint16_t weatherColor;
        switch (weatherType) {
          case WeatherType::Sun:     weatherColor = COLOR_WEATHER_SUN; break;
          case WeatherType::Moon:    weatherColor = COLOR_WEATHER_MOON; break;
          case WeatherType::Cloudy:  weatherColor = COLOR_WEATHER_CLOUD; break;
          case WeatherType::Rain:    weatherColor = COLOR_WEATHER_RAIN; break;
          case WeatherType::Thunder: weatherColor = COLOR_WEATHER_THUNDER; break;
          default:                   weatherColor = COLOR_WEATHER_CLOUD; break;
        }
        if (isNightStandActive) weatherColor = COLOR_NIGHT_AMBER;
        uint16_t activeWeatherColor = blendColor565(bgColor, weatherColor, sensorAlpha);

        drawWeatherIconToBand(cxWeather, cyWeather, weatherType, pitchRad, iconScale, yStart, activeWeatherColor);
      }

      // Ambient Weather Gravity Particles
      for (int i = 0; i < weatherDrawCount; i++) {
        uint16_t partCol = blendColor565(bgColor, isNightStandActive ? COLOR_NIGHT_AMBER : weatherDrawList[i].color, sensorAlpha);
        drawLineToBand(weatherDrawList[i].x0, weatherDrawList[i].y0,
                       weatherDrawList[i].x1, weatherDrawList[i].y1,
                       yStart, partCol);
      }

      // Draw Temperature Gauge (Dynamic aperture bloom scale during transition)
      int animTempFill = static_cast<int>(tempFillCount * sensorWeight);
      for (int i = 0; i < NUM_GAUGE_STEPS; i++) {
        uint16_t c = (i < animTempFill) ? primaryColor : COLOR_DARKGRAY;
        for (int t = 0; t < 3; t++) {
          int py = tempGaugeY[i][t];
          if (py >= yStart && py <= yEnd) {
            int px = tempGaugeX[i][t];
            if (static_cast<unsigned>(px) < SCREEN_WIDTH) {
              screenBand[(py - yStart) * SCREEN_WIDTH + px] = c;
            }
          }
        }
      }

      // Draw Humidity Gauge (Dynamic aperture bloom scale during transition)
      int animHumFill = static_cast<int>(humFillCount * sensorWeight);
      uint16_t humActiveColor = activeAccent;
      for (int i = 0; i < NUM_GAUGE_STEPS; i++) {
        uint16_t c = (i < animHumFill) ? humActiveColor : COLOR_DARKGRAY;
        for (int t = 0; t < 3; t++) {
          int py = humGaugeY[i][t];
          if (py >= yStart && py <= yEnd) {
            int px = humGaugeX[i][t];
            if (static_cast<unsigned>(px) < SCREEN_WIDTH) {
              screenBand[(py - yStart) * SCREEN_WIDTH + px] = c;
            }
          }
        }
      }
    }

    // 3. Mode 3 (Settings Rotary Dial) rendered according to settingsWeight
    if (settingsWeight > 0.01f) {
      uint16_t setAlpha = static_cast<uint16_t>(roundf(settingsWeight * 255.0f));
      uint16_t setColWhite = blendColor565(bgColor, primaryColor, setAlpha);
      uint16_t currentTheme = getThemeAccentColor();
      uint16_t activeAccent = isNightStandActive ? COLOR_NIGHT_AMBER : currentTheme;
      uint16_t setColAccent = blendColor565(bgColor, activeAccent, setAlpha);
      uint16_t setColGray   = blendColor565(bgColor, COLOR_DARKGRAY, setAlpha);

      // A. Knurled Outer Hash Marks (arc from -90 deg to +90 deg, dense, shortened)
      for (int i = 0; i < SETTINGS_KNURL_TICKS; i++) {
        uint16_t col = knurlTicks[i].isMajor ? setColWhite : setColGray;
        drawLineToBand(knurlTicks[i].x1, knurlTicks[i].y1, knurlTicks[i].x2, knurlTicks[i].y2, yStart, col);
      }

      // B. Apex Index Pointer Line (At 12 o'clock, 2px bold)
      for (int off = 0; off <= 1; off++) {
        drawLineToBand(apexX1[off], apexY1[off], apexX2[off], apexY2[off], yStart, setColWhite);
      }

      // C. Rotary Badges along the arc
      for (int i = 0; i < visibleSettingCount; i++) {
        const SettingDrawItem& item = settingDrawList[i];
        if (item.isApex) {
          // Solid filled pill capsule badge with dark text for high-contrast legibility
          drawCapsuleToBand(item.x, item.y, SETTINGS_CAPSULE_HW, SETTINGS_CAPSULE_HH, pitchRad, yStart, setColWhite);
          drawRotatedScaledStringToBand(item.x, item.y, SETTING_NAMES[item.index], pitchRad, 1.25f, yStart, COLOR_BLACK);
        } else if (!inSubMenu) {
          // Circular Badge with single character (hidden during sub-menu to focus user on adjustments)
          drawCircleToBand(item.x, item.y, 9, yStart, setColWhite);
          char chStr[2] = { SETTING_BADGE_CHARS[item.index], '\0' };
          drawRotatedScaledStringToBand(item.x, item.y, chStr, pitchRad, 1.20f, yStart, COLOR_BLACK);
        }
      }

      // D. Center Settings Information Card
      int activeIdx = currentSettingIndex;
      int cxMid = 120 + static_cast<int>(roundf(10.0f * sa_norm));
      int cyMid = 120 - static_cast<int>(roundf(10.0f * ca_norm));

      int cyTitle = cyMid - 18;
      int cyVal   = cyMid + 4;
      int cyDesc  = cyMid + 25;

      const char* titleStr = SETTING_TITLES[activeIdx];
      const char* valStr = "";
      const char* descStr = SETTING_DESCS[activeIdx];
      char customVal[24] = "";

      if (inSubMenu) {
        switch (activeIdx) {
          case 0: // BRIGHT
            titleStr = "SET BRIGHTNESS";
            valStr = BRIGHTNESS_LABELS[brightnessStepIndex];
            descStr = "TAP: CYCLE | 2X: BACK";
            break;

          case 1: // GLANCE
            titleStr = "SET AUTO GLANCE";
            valStr = autoPeekEnabled ? "ON (60s)" : "OFF (LOCKED)";
            descStr = "TAP: TOGGLE | 2X: BACK";
            break;

          case 2: // NIGHT
            titleStr = "SET NIGHT STAND";
            valStr = nightStandEnabled ? "ENABLED (DIM)" : "DISABLED";
            descStr = "TAP: TOGGLE | 2X: BACK";
            break;

          case 3: // GYRO
            titleStr = "GYRO TILT TRACK";
            valStr = gyroTrackingEnabled ? "DYNAMIC (TILT)" : "LOCKED (0 DEG)";
            descStr = "TAP: TOGGLE | 2X: BACK";
            break;

          case 4: // THEME
            titleStr = "SET THEME ACCENT";
            valStr = THEME_NAMES[themeIndex % 4];
            descStr = "TAP: CYCLE | 2X: BACK";
            break;

          case 5: // INFO
            titleStr = "DEVICE INFO";
            switch (infoSubIndex) {
              case 0:
                snprintf(customVal, sizeof(customVal), "%s", (wifiIp && wifiIp[0]) ? wifiIp : "Offline");
                break;
              case 1:
                snprintf(customVal, sizeof(customVal), "%d dBm", wifiRssi);
                break;
              case 2:
                snprintf(customVal, sizeof(customVal), "Free: %uB", ESP.getFreeHeap());
                break;
              case 3:
                snprintf(customVal, sizeof(customVal), "Clock: %uMHz", ESP.getCpuFreqMHz());
                break;
              case 4: {
                unsigned long secs = millis() / 1000;
                snprintf(customVal, sizeof(customVal), "Up: %lum %lus", secs / 60, secs % 60);
                break;
              }
              case 5: {
                if (!isnan(temperature)) {
                  snprintf(customVal, sizeof(customVal), "DHT: %.1f^C %.0f%%", temperature, !isnan(humidity) ? humidity : 0.0f);
                } else {
                  snprintf(customVal, sizeof(customVal), "DHT: Reading...");
                }
                break;
              }
            }
            valStr = customVal;
            descStr = "TAP: NEXT | 2X: BACK";
            break;

          case 6: // EXIT
            titleStr = "EXIT MENU";
            valStr = "BACK TO CLOCK";
            descStr = "TAP / 2X: HOME";
            break;

          default:
            break;
        }
      } else {
        switch (activeIdx) {
          case 0: // BRIGHT
            valStr = BRIGHTNESS_LABELS[brightnessStepIndex];
            break;
          case 1: // GLANCE
            valStr = autoPeekEnabled ? "ON (60s)" : "OFF";
            break;
          case 2: // NIGHT
            valStr = nightStandEnabled ? "ENABLED" : "DISABLED";
            break;
          case 3: // GYRO
            valStr = gyroTrackingEnabled ? "DYNAMIC" : "LOCKED 0*";
            break;
          case 4: // THEME
            valStr = THEME_NAMES[themeIndex % 4];
            break;
          case 5: // INFO
            valStr = (wifiIp && wifiIp[0]) ? wifiIp : "DIAG";
            break;
          case 6: // EXIT
            valStr = "BACK TO CLOCK";
            break;
        }
      }

      drawRotatedScaledStringToBand(cxMid, cyTitle, titleStr, pitchRad, 1.15f, yStart, setColAccent);
      drawRotatedScaledStringToBand(cxMid, cyVal, valStr, pitchRad, inSubMenu ? 2.2f : 1.9f, yStart, setColWhite);
      drawRotatedScaledStringToBand(cxMid, cyDesc, descStr, pitchRad, 0.95f, yStart, setColAccent);

      // E. Pager / Step Dots Indicator
      if (!inSubMenu) {
        for (int d = 0; d < SETTINGS_NUM_ITEMS; d++) {
          bool isCurDot = (d == activeIdx);
          drawCircleToBand(pagerDotX[d], pagerDotY[d], isCurDot ? 2 : 1, yStart, isCurDot ? setColAccent : setColGray);
        }
      } else if (activeIdx == 0) {
        int cySubDots = cyMid + 35;
        for (int b = 0; b < BRIGHTNESS_LEVEL_COUNT; b++) {
          float dotOff = (static_cast<float>(b) - 1.5f) * 12.0f;
          int bx = cxMid + static_cast<int>(roundf(dotOff * ca_norm));
          int by = cySubDots + static_cast<int>(roundf(dotOff * sa_norm));
          bool isCurB = (b == brightnessStepIndex);
          drawCircleToBand(bx, by, isCurB ? 3 : 1, yStart, isCurB ? setColAccent : setColGray);
        }
      } else if (activeIdx == 4) {
        int cySubDots = cyMid + 35;
        for (int b = 0; b < 4; b++) {
          float dotOff = (static_cast<float>(b) - 1.5f) * 12.0f;
          int bx = cxMid + static_cast<int>(roundf(dotOff * ca_norm));
          int by = cySubDots + static_cast<int>(roundf(dotOff * sa_norm));
          bool isCurB = (b == (themeIndex % 4));
          uint16_t dotC = blendColor565(bgColor, THEME_COLORS[b], setAlpha);
          drawCircleToBand(bx, by, isCurB ? 3 : 1, yStart, dotC);
        }
      } else if (activeIdx == 5) {
        int cySubDots = cyMid + 35;
        for (int b = 0; b < 6; b++) {
          float dotOff = (static_cast<float>(b) - 2.5f) * 10.0f;
          int bx = cxMid + static_cast<int>(roundf(dotOff * ca_norm));
          int by = cySubDots + static_cast<int>(roundf(dotOff * sa_norm));
          bool isCurB = (b == infoSubIndex);
          drawCircleToBand(bx, by, isCurB ? 2 : 1, yStart, isCurB ? setColAccent : setColGray);
        }
      }
    }

    tft.drawRGBBitmap(0, yStart, screenBand, SCREEN_WIDTH, BAND_HEIGHT);

    // Yield CPU time to ESP8266 background WiFi and hardware watchdog between bands
    yield();
  }
}
