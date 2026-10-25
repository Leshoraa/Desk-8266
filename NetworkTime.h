#ifndef NETWORK_TIME_H
#define NETWORK_TIME_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <time.h>

class NetworkTime {
public:
  using ProgressCallback = void (*)(const char* statusText, int percent, const char* detailText);

  NetworkTime();

  void begin(ProgressCallback onProgress = nullptr);
  bool isConnected() const;
  String getLocalIPString() const;
  void getCurrentTime(int& hour, int& minute, int& second, unsigned long& subSecondMs);

private:
  bool connected;
  time_t lastObservedSec;
  unsigned long secondStartMs;
};

#endif // NETWORK_TIME_H
