#pragma once
#include <Arduino.h>

// Fan speed via supply voltage: bidirectional PWM current injection into the
// D-SUN MP1584EN's FB node (see docs/buck-fb-control.md). The PWM->Vout
// mapping is inverted around the pot-calibrated anchor BUCK_VOUT_CAL (12 V =
// zero injection at ~24% duty, also the high-Z/boot state): ~8% duty sinks
// FB current for the 14 V max, 100% duty sources for the ~2.4 V floor.
// setPowerPercent() takes fan power 0..100%; 0 drops the buck to
// BUCK_VOUT_MIN (fan stops), >0 maps linearly onto FAN_V_MIN..FAN_V_MAX.
class FanControl {
public:
  void begin(); // call FIRST in setup(); until then the buck sits at the 12 V anchor
  void setPowerPercent(float pct);
  float powerPercent() const { return _power; }
  float targetVolts() const { return _volts; }

private:
  void applyVolts(float v);
  float _power = 0;
  float _volts = 0;
};
