#pragma once
#include <Arduino.h>

// 25 kHz PWM on an open-drain pin; the Noctua PWM line has an internal
// pull-up to 5V, so no level shifter is needed.
class FanControl {
public:
  void begin();
  void setDutyPercent(float pct);
  float dutyPercent() const { return _duty; }

private:
  float _duty = 0;
};
