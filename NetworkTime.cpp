#include "NetworkTime.h"
#include "Config.h"

NetworkTime::NetworkTime()
  : connected(false),
    lastObservedSec(0),
    secondStartMs(0) {}

void NetworkTime::begin(ProgressCallback onProgress) {
  WiFi.persistent(false);
  WiFi.disconnect(true);
  delay(120);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable sleep mode for reliable low-latency packet delivery
  WiFi.setOutputPower(20.5);          // Maximum transmission power 20.5 dBm
  WiFi.setAutoReconnect(true);

  if (onProgress) {
    onProgress("Connecting", 0, nullptr);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t startMs = millis();
  int lastReportedPercent = -1;
  bool reconnectAttempted = false;

  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < WIFI_TIMEOUT_MS) {
    uint32_t elapsed = millis() - startMs;

    if (elapsed > 7000 && !reconnectAttempted) {
      reconnectAttempted = true;
      WiFi.reconnect();
      if (onProgress) {
        onProgress("Reconnecting", 35, nullptr);
      }
    }

    int percent = (int)((elapsed * 95UL) / WIFI_TIMEOUT_MS);
    if (percent > 95) percent = 95;

    if (percent != lastReportedPercent) {
      lastReportedPercent = percent;
      if (onProgress) {
        onProgress(reconnectAttempted ? "Reconnecting" : "Connecting", percent, nullptr);
      }
    }

    delay(40);
    yield();
  }

  connected = (WiFi.status() == WL_CONNECTED);

  if (connected) {
    String ipString = "IP: " + WiFi.localIP().toString();
    if (onProgress) {
      onProgress("Connected", 100, ipString.c_str());
    }

    // Configure NTP time synchronization for WIB (UTC+7)
    configTime(TIMEZONE_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_HOST_1, NTP_HOST_2, NTP_HOST_3);

    // Give background network task 2 seconds to complete initial SNTP packet exchange
    uint32_t syncWaitStart = millis();
    while ((millis() - syncWaitStart) < 2000) {
      delay(50);
      yield();
    }
  } else {
    if (onProgress) {
      onProgress("Disconnected", 0, "Offline Mode");
    }
    delay(1800);
  }
}

bool NetworkTime::isConnected() const {
  return connected;
}

String NetworkTime::getLocalIPString() const {
  if (connected) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

void NetworkTime::getCurrentTime(int& hour, int& minute, int& second, unsigned long& subSecondMs,
                                 char* dateBuffer, size_t bufferSize) {
  time_t nowSec = time(nullptr);
  struct tm* timeInfo = localtime(&nowSec);
  hour   = timeInfo ? timeInfo->tm_hour : 0;
  minute = timeInfo ? timeInfo->tm_min  : 0;
  second = timeInfo ? timeInfo->tm_sec  : 0;

  if (dateBuffer && bufferSize >= 8) {
    static const char* const DAY_ABBREV[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    int wday = (timeInfo && timeInfo->tm_wday >= 0 && timeInfo->tm_wday < 7) ? timeInfo->tm_wday : 0;
    int mday = timeInfo ? timeInfo->tm_mday : 1;
    snprintf(dateBuffer, bufferSize, "%s %d", DAY_ABBREV[wday], mday);
  }

  unsigned long nowMs = millis();
  if (nowSec != lastObservedSec) {
    lastObservedSec = nowSec;
    secondStartMs = nowMs;
  }

  unsigned long elapsedMs = nowMs - secondStartMs;
  if (elapsedMs > 999) {
    elapsedMs = 999;
  }
  subSecondMs = elapsedMs;
}
