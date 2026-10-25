#include "WeatherService.h"
#include <WiFiClient.h>

namespace {
constexpr const char* METEO_HOST = "api.open-meteo.com";
constexpr uint16_t METEO_PORT    = 80;
}

WeatherService::WeatherService()
  : currentCondition(WeatherType::Cloudy),
    isDaytime(true),
    externalTemperature(28.0f),
    hasData(false),
    lastPollTimestamp(0),
    initialFetchAttempted(false) {}

void WeatherService::update() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  // Guard startup: wait 5s after boot so WiFi connection is fully stabilized
  if (now < 5000) {
    return;
  }

  // If no data established yet, retry every 30s without starving main loop
  if (!hasData) {
    if (now - lastPollTimestamp >= 30000 || lastPollTimestamp == 0) {
      lastPollTimestamp = now;
      fetchWeatherData();
    }
    return;
  }

  if (now - lastPollTimestamp >= WEATHER_UPDATE_INTERVAL) {
    lastPollTimestamp = now;
    fetchWeatherData();
  }
}

void WeatherService::fetchWeatherData() {
  yield();
  WiFiClient client;
  client.setTimeout(1000); // 1.0s max timeout prevents watchdog resets

  if (!client.connect(METEO_HOST, METEO_PORT)) {
    client.stop();
    yield();
    return;
  }
  yield();

  // HTTP GET for current weather code, precipitation, rain, showers, cloud cover, and temperature in Sleman
  client.print(F("GET /v1/forecast?latitude="));
  client.print(WEATHER_LATITUDE, 3);
  client.print(F("&longitude="));
  client.print(WEATHER_LONGITUDE, 3);
  client.print(F("&current=weather_code,is_day,precipitation,rain,showers,cloud_cover,temperature_2m&timezone=Asia%2FJakarta HTTP/1.1\r\nHost: "));
  client.print(METEO_HOST);
  client.print(F("\r\nConnection: close\r\n\r\n"));

  yield();

  // Stream past headers and metadata directly until reaching the current weather block
  if (!client.find(const_cast<char*>("\"current\":{"))) {
    client.stop();
    yield();
    return;
  }
  yield();

  // Read only the current weather object payload with ample buffer headroom (256 bytes)
  char buffer[256];
  size_t bytesRead = client.readBytesUntil('}', buffer, sizeof(buffer) - 1);
  buffer[bytesRead] = '\0';
  client.stop();
  yield();

  char* pCode    = strstr(buffer, "\"weather_code\":");
  char* pDay     = strstr(buffer, "\"is_day\":");
  char* pPrecip  = strstr(buffer, "\"precipitation\":");
  char* pRain    = strstr(buffer, "\"rain\":");
  char* pShowers = strstr(buffer, "\"showers\":");
  char* pCloud   = strstr(buffer, "\"cloud_cover\":");
  char* pTemp    = strstr(buffer, "\"temperature_2m\":");

  if (!pCode) {
    return;
  }

  int code = atoi(pCode + 15);
  int day  = pDay ? atoi(pDay + 9) : 1;
  isDaytime = (day != 0);

  float precip  = pPrecip  ? static_cast<float>(atof(pPrecip + 16)) : 0.0f;
  float rain    = pRain    ? static_cast<float>(atof(pRain + 7))     : 0.0f;
  float showers = pShowers ? static_cast<float>(atof(pShowers + 10)) : 0.0f;
  int cloud     = pCloud   ? atoi(pCloud + 14) : 0;

  if (pTemp) {
    externalTemperature = static_cast<float>(atof(pTemp + 17));
  }

  // Calibrated weather classification:
  // Prevents false rain triggers when it is overcast/mendung with negligible trace moisture
  if (code >= 95) {
    currentCondition = WeatherType::Thunder; // Thunderstorm / Petir
  } else if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82) || rain >= 0.3f || (showers >= 0.4f && precip >= 0.4f) || (code >= 51 && code <= 55 && precip >= 0.5f)) {
    currentCondition = WeatherType::Rain; // Tangible rain (>0.3mm/hr or moderate/heavy showers)
  } else if (code == 2 || code == 3 || code == 45 || code == 48 || (code >= 51 && code <= 55) || cloud >= 30) {
    currentCondition = WeatherType::Cloudy; // Overcast or cloudy (mendung / berawan)
  } else if (code <= 1 && cloud < 30) {
    currentCondition = isDaytime ? WeatherType::Sun : WeatherType::Moon; // Clear sky (Cerah)
  } else {
    currentCondition = WeatherType::Cloudy;
  }

  hasData = true;
}

WeatherType WeatherService::getCondition() const {
  return currentCondition;
}

bool WeatherService::isDay() const {
  return isDaytime;
}

float WeatherService::getExternalTemperature() const {
  return externalTemperature;
}

bool WeatherService::hasValidData() const {
  return hasData;
}
