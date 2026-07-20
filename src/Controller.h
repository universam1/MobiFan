#pragma once
#include <Arduino.h>
#include "ButtonInput.h"
#include "config.h"

enum class Mode { Manual, Auto };

// Mode/level state machine and duty computation.
// Manual: levels 0..5 as fixed duties.
// Auto: levels 1..5 select a ramp; duty runs continuously from the minimum
// at <=15C up to 100% at the ramp's max temperature (40/35/30/25/20C).
class Controller {
public:
  // Returns true if level or mode changed (drives the UI popup).
  bool handleButton(ButtonEvent ev);
  float computeDutyPercent(float tempC, bool tempValid) const;

  Mode mode() const { return _mode; }
  uint8_t level() const { return _mode == Mode::Auto ? _autoLevel : _manualLevel; }

private:
  Mode _mode = Mode::Auto;
  uint8_t _manualLevel = 0;
  uint8_t _autoLevel = BOOT_AUTO_LEVEL;
};
