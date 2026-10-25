#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Config.h"
#include "weather_icons.h"

class WeatherService {
public:
  WeatherService();

  void update();
  WeatherType getCondition() const;
  bool isDay() const;
  float getExternalTemperature() const;
  bool hasValidData() const;

private:
  void fetchWeatherData();

  WeatherType currentCondition;
  bool isDaytime;
  float externalTemperature;
  bool hasData;
  unsigned long lastPollTimestamp;
  bool initialFetchAttempted;
};

#endif // WEATHER_SERVICE_H
