#pragma once
#include <Arduino.h>

// Measures fan RPM from the open-collector tach signal (2 pulses per rev).
//
// The ISR timestamps every accepted falling edge and accumulates the intervals
// between them; tick() converts the mean interval into RPM. Deliberately NOT a
// pulse count per fixed window: at 2 pulses/rev a 1 s counting window resolves
// only 30 RPM per pulse, so the unsynchronized window edge made the reading
// dither by +-30 RPM. Period measurement is limited by micros() granularity
// instead (~0.03 RPM at 900 RPM). See config.h for the tunables.
class Tach {
public:
  void begin();
  void tick(uint32_t now);
  uint32_t rpm() const { return _rpm; }

private:
  static void IRAM_ATTR isr();
  // ISR state must be static: attachInterrupt() takes a plain function pointer,
  // so a second Tach instance would share these (single-fan design).
  static volatile uint32_t _sumUs;      // sum of accepted intervals this window
  static volatile uint32_t _intervals;  // number of those intervals
  static volatile uint32_t _lastEdgeUs; // timestamp of last accepted edge
  static volatile bool     _haveEdge;   // false until the first edge arrives
  float    _rpmEma    = 0.0f;
  uint32_t _rpm       = 0;
  uint32_t _lastUpdate = 0;
};
