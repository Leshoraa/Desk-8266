#include "DisplayRenderer.h"
#include "font5x7.h"

namespace {
const char* const HOUR_LABELS[12] = {
  "12", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"
};
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
    isWiFiStaticDrawn(false) {}

void DisplayRenderer::begin() {
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  tft.begin(40000000); // 40MHz maximum stable SPI clock for GC9A01
  tft.setRotation(0);
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
  tft.fillScreen(COLOR_BLACK);
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

void DisplayRenderer::drawCenterCapToBand(int cx, int cy, int yStart, uint16_t color) {
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
            screenBand[rowOffset + px] = (distSq <= 1) ? COLOR_BLACK : color;
          }
        }
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

void DisplayRenderer::renderFrame(float pitchDeg, bool isTouched, float temperature, float humidity,
                                  int hour, int minute, int second, unsigned long subSecondMs, AppMode mode) {
  unsigned long nowMs = millis();
  float pitchRad = pitchDeg * (3.14159265f / 180.0f);

  float renderDelta = fabsf(pitchDeg - lastRenderedPitch);
  if (renderDelta > 180.0f) renderDelta = fabsf(renderDelta - 360.0f);

  bool shouldRedraw = false;
  if (mode != lastRenderedMode) shouldRedraw = true;
  if (isTouched != lastRenderedTouch) shouldRedraw = true;
  if (renderDelta >= 1.0f) shouldRedraw = true;

  if (mode == AppMode::Clock) {
    // 35 FPS continuous refresh drives smooth sub-second dock magnification wave
    if (nowMs - lastClockRenderMs >= 28) {
      shouldRedraw = true;
    }
  } else {
    if (fabsf(temperature - lastRenderedTemp) >= 0.15f || fabsf(humidity - lastRenderedHum) >= 0.5f) {
      shouldRedraw = true;
    }
  }

  if (!shouldRedraw) {
    return;
  }

  lastClockRenderMs = nowMs;
  lastRenderedPitch = pitchDeg;
  lastRenderedTouch = isTouched;
  lastRenderedTemp  = temperature;
  lastRenderedHum   = humidity;
  lastRenderedMode  = mode;

  uint16_t primaryColor = isTouched ? COLOR_GREEN : COLOR_WHITE;
  uint16_t tickColor    = isTouched ? COLOR_GREEN : COLOR_WHITE;
  uint16_t accentColor  = isTouched ? COLOR_AMBER : COLOR_WHITE;

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

  // Sub-second continuous floating point position driving dock magnification wave
  float sValAnim = static_cast<float>(second) + (static_cast<float>(subSecondMs) / 1000.0f);
  float sPos = sValAnim / 5.0f;

  char tempString[12];
  char humString[16];
  if (mode == AppMode::Sensors) {
    if (!isnan(temperature)) {
      snprintf(tempString, sizeof(tempString), "%4.1f ^C", temperature);
    } else {
      snprintf(tempString, sizeof(tempString), "--.- ^C");
    }
    if (!isnan(humidity)) {
      snprintf(humString, sizeof(humString), "HUM: %2.0f%%", humidity);
    } else {
      snprintf(humString, sizeof(humString), "HUM: --%");
    }
  }

  float sa_norm = sinf(pitchRad);
  float ca_norm = cosf(pitchRad);

  // Render 6 horizontal bands sequentially (240x240 screen, 19.2KB RAM, 100% flicker-free)
  for (int band = 0; band < NUM_BANDS; band++) {
    int yStart = band * BAND_HEIGHT;
    memset(screenBand, 0, sizeof(screenBand));

    // 1. Outer Gyro Dial Tick Marks (R=104..114)
    for (int i = 0; i < 12; i++) {
      float angle = (i * (3.14159265f / 6.0f)) - 1.5707963f + pitchRad;
      float ca = cosf(angle);
      float sa = sinf(angle);

      int rIn = (i % 3 == 0) ? GYRO_TICK_MAJOR_INNER : GYRO_TICK_MINOR_INNER;
      int rOut = GYRO_TICK_OUTER_RADIUS;

      int x1 = static_cast<int>(roundf(120.0f + rIn  * ca));
      int y1 = static_cast<int>(roundf(120.0f + rIn  * sa));
      int x2 = static_cast<int>(roundf(120.0f + rOut * ca));
      int y2 = static_cast<int>(roundf(120.0f + rOut * sa));

      uint16_t col = (i == 0) ? primaryColor : ((i % 3 == 0) ? accentColor : tickColor);
      drawLineToBand(x1, y1, x2, y2, yStart, col);
    }

    if (mode == AppMode::Clock) {
      // 2. Hour Digits: The next number begins appearing strictly when the hand lands on the current number.
      // At any moment in interval [curIdx, nextIdx]:
      // - curIdx shrinks from 2.0x -> 0.0x as the hand leaves it.
      // - nextIdx grows from 0.0x -> 2.0x as the hand approaches it.
      // - All other numbers are strictly hidden.
      float pos = sPos;
      while (pos >= 12.0f) pos -= 12.0f;
      while (pos < 0.0f)   pos += 12.0f;

      int curIdx = static_cast<int>(floorf(pos));
      if (curIdx >= 12) curIdx = 11;
      int nextIdx = (curIdx + 1) % 12;

      float t = pos - static_cast<float>(curIdx);
      if (t < 0.0f) t = 0.0f;
      if (t > 1.0f) t = 1.0f;

      float factorCur  = 0.5f * (1.0f + cosf(t * 3.14159265f));
      float factorNext = 0.5f * (1.0f - cosf(t * 3.14159265f));

      for (int k = 0; k < 2; k++) {
        int idx = (k == 0) ? curIdx : nextIdx;
        float mFactor = (k == 0) ? factorCur : factorNext;

        if (mFactor <= 0.015f) {
          continue;
        }

        float numAngle = (idx * (3.14159265f / 6.0f)) - 1.5707963f + pitchRad;

        // Continuous fractional scaling: smoothly expands/shrinks between 0.70x and 2.00x
        float scale = 0.70f + (mFactor * 1.30f);

        // Radial glide: lifts smoothly from 84px to 91px
        float rNum = HOUR_NUMBER_BASE_R + (mFactor * HOUR_NUMBER_LIFT_R);
        int nx = static_cast<int>(roundf(120.0f + rNum * cosf(numAngle)));
        int ny = static_cast<int>(roundf(120.0f + rNum * sinf(numAngle)));

        // Smooth brightness fade: from soft gray (RGB 60) to brilliant white (RGB 255)
        uint16_t numColor;
        if (isTouched) {
          numColor = COLOR_GREEN;
        } else {
          uint8_t bright = static_cast<uint8_t>(60.0f + 195.0f * mFactor);
          numColor = tft.color565(bright, bright, bright);
        }

        drawRotatedScaledStringToBand(nx, ny, HOUR_LABELS[idx], pitchRad, scale, yStart, numColor);
      }

      // 3. Analog Clock Hands (Symmetric Thickness, Zero Slant, Non-overlapping)
      // Hour Hand: 40px length, 2px symmetric width
      for (int k = 0; k < 2; k++) {
        float off = (k == 0) ? -0.5f : 0.5f;
        int x0 = static_cast<int>(roundf(120.0f + off * px_h));
        int y0 = static_cast<int>(roundf(120.0f + off * py_h));
        int x1 = static_cast<int>(roundf(120.0f + HOUR_HAND_LENGTH * ca_h + off * px_h));
        int y1 = static_cast<int>(roundf(120.0f + HOUR_HAND_LENGTH * sa_h + off * py_h));
        drawLineToBand(x0, y0, x1, y1, yStart, primaryColor);
      }

      // Minute Hand: 58px length, 2px symmetric width
      for (int k = 0; k < 2; k++) {
        float off = (k == 0) ? -0.5f : 0.5f;
        int x0 = static_cast<int>(roundf(120.0f + off * px_m));
        int y0 = static_cast<int>(roundf(120.0f + off * py_m));
        int x1 = static_cast<int>(roundf(120.0f + MINUTE_HAND_LENGTH * ca_m + off * px_m));
        int y1 = static_cast<int>(roundf(120.0f + MINUTE_HAND_LENGTH * sa_m + off * py_m));
        drawLineToBand(x0, y0, x1, y1, yStart, primaryColor);
      }

      // Second Hand: 70px length, 12px tail counterweight
      int x_tail = static_cast<int>(roundf(120.0f - SECOND_HAND_TAIL_LEN * ca_s));
      int y_tail = static_cast<int>(roundf(120.0f - SECOND_HAND_TAIL_LEN * sa_s));
      int x_tip  = static_cast<int>(roundf(120.0f + SECOND_HAND_LENGTH * ca_s));
      int y_tip  = static_cast<int>(roundf(120.0f + SECOND_HAND_LENGTH * sa_s));
      drawLineToBand(x_tail, y_tail, x_tip, y_tip, yStart, primaryColor);

      // Center Pin Cap (Radius 3)
      drawCenterCapToBand(SCREEN_CENTER_X, SCREEN_CENTER_Y, yStart, primaryColor);

    } else {
      // Mode 2: Sensor Dashboard (Matching original layout)
      int cxTemp = 120 + static_cast<int>(roundf(12.0f * sa_norm));
      int cyTemp = 120 - static_cast<int>(roundf(12.0f * ca_norm));
      int cxHum  = 120 - static_cast<int>(roundf(14.0f * sa_norm));
      int cyHum  = 120 + static_cast<int>(roundf(14.0f * ca_norm));

      drawRotatedStringToBand(cxTemp, cyTemp, tempString, pitchRad, 2, yStart, primaryColor);
      drawRotatedStringToBand(cxHum,  cyHum,  humString,  pitchRad, 1, yStart, primaryColor);
    }

    tft.drawRGBBitmap(0, yStart, screenBand, SCREEN_WIDTH, BAND_HEIGHT);

    // Yield CPU time to ESP8266 background WiFi and hardware watchdog between bands
    yield();
  }
}
