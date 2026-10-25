#ifndef ENVIRONMENT_SENSOR_H
#define ENVIRONMENT_SENSOR_H

#include <Arduino.h>
#include <DHT.h>

class EnvironmentSensor {
public:
  EnvironmentSensor();

  void begin(uint8_t dataPin);
  void update();
  float getTemperature() const;
  float getHumidity() const;

private:
  DHT dht;
  float cachedTemperature;
  float cachedHumidity;
  unsigned long lastPollTimestamp;
};

#endif // ENVIRONMENT_SENSOR_H
