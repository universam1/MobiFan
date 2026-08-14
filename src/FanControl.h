#pragma once
#include <Arduino.h>

// Fan speed via USB-C PD voltage selection: 4 discrete steps (5/9/12/15V)
// driven by a CH224K PD sink chip's CFG1-3 pins (see docs/pd-sink-control.md).
// setPowerPercent() quantizes the continuous 0..100% to the nearest voltage
// step; targetVolts() returns the actual step voltage.
class FanControl {
public:
  void begin();              // call FIRST in setup(); until then CFG1 floats -> 5V default
  void setPowerPercent(float pct);
  float powerPercent() const { return _power; }
  float targetVolts()  const { return _volts; }

private:
  void applyCfg(float volts);
  float _power = 0;
  float _volts = 5.0f;
};
