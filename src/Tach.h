#pragma once
#include <Arduino.h>

// Counts open-collector tach pulses (2 per revolution) and computes RPM
// over a 1-second window.
class Tach {
public:
  void begin();
  void tick(uint32_t now);
  uint32_t rpm() const { return _rpm; }

private:
  static void IRAM_ATTR isr();
  static volatile uint32_t _pulses;
  uint32_t _rpm = 0;
  uint32_t _lastWindow = 0;
};
