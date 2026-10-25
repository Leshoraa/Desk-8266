#include "EnvironmentSensor.h"
#include "Config.h"

EnvironmentSensor::EnvironmentSensor()
  : dht(PIN_DHT22_DATA, DHT22),
    cachedTemperature(NAN),
    cachedHumidity(NAN),
    lastPollTimestamp(0) {}

void EnvironmentSensor::begin(uint8_t dataPin) {
  (void)dataPin; // Pin is configured during object construction
  dht.begin();
}

void EnvironmentSensor::update() {
  unsigned long now = millis();
  if (now - lastPollTimestamp < 1000) {
    return;
  }
  lastPollTimestamp = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t)) {
    cachedTemperature = t;
  }
  if (!isnan(h)) {
    cachedHumidity = h;
  }
}

float EnvironmentSensor::getTemperature() const {
  return cachedTemperature;
}

float EnvironmentSensor::getHumidity() const {
  return cachedHumidity;
}
