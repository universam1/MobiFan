#pragma once
#include <Arduino.h>

// Reads the NTC divider once per second (8x oversampled) and keeps an
// EMA-smoothed temperature in Celsius.
class TempSensor {
public:
  void begin();
  void tick(uint32_t now);
  float celsius() const { return _temp; }
  bool valid() const { return _valid; }

private:
  float readOnce() const;
  float _temp = 0;
  bool _valid = false;
  uint32_t _lastSample = 0;
};
