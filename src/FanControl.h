#pragma once
#include <Arduino.h>

// Fan speed via supply voltage: bidirectional PWM current injection into the
// MT3608 boost's FB node (see docs/boost-fb-control.md). The PWM->Vout
// mapping is inverted around the pot-calibrated anchor BOOST_VOUT_CAL (12 V =
// zero injection at ~18.2% duty, also the high-Z/boot state): ~4.8% duty sinks
// FB current for the 14 V max, ~61.5% duty sources for the 5.5 V floor.
// setPowerPercent() takes fan power 0..100%; 0 drops the boost to
// BOOST_VOUT_MIN (fan's lowest voltage -- there is no true off), >0 maps
// linearly onto FAN_V_MIN..FAN_V_MAX.
class FanControl {
public:
  void begin(); // call FIRST in setup(); until then the boost sits at the 12 V anchor
  void setPowerPercent(float pct);
  float powerPercent() const { return _power; }
  float targetVolts() const { return _volts; }


private:
  void applyVolts(float v);
  float _power = 0;
  float _volts = 0;
};
