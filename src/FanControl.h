#pragma once
#include <Arduino.h>

// Fan speed via supply voltage: PWM current injection into an MP1584EN buck
// converter's FB node (see docs/buck-fb-control.md). The PWM->Vout mapping is
// inverted: 0% duty = ~14 V (also the unpowered/boot state!), 100% duty =
// ~3.2 V. setPowerPercent() takes fan power 0..100%; 0 drops the buck to its
// minimum voltage (fan stops), >0 maps linearly onto FAN_V_MIN..FAN_V_MAX.
class FanControl {
public:
  void begin(); // call FIRST in setup() to end the ~14 V boot transient asap
  void setPowerPercent(float pct);
  float powerPercent() const { return _power; }
  float targetVolts() const { return _volts; }

private:
  void applyVolts(float v);
  float _power = 0;
  float _volts = 0;
};
