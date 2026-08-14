#include "Controller.h"
#include "config.h"

bool Controller::handleButton(ButtonEvent ev) {
  if (ev == ButtonEvent::Long) {
    _mode = (_mode == Mode::Auto) ? Mode::Manual : Mode::Auto;
    return true;
  }
  if (ev == ButtonEvent::Short) {
    if (_mode == Mode::Manual)
      _manualLevel = _manualLevel % MANUAL_LEVEL_COUNT + 1;    // 1..N, same as auto
    else
      _autoLevel = _autoLevel % AUTO_LEVEL_COUNT + 1;          // 1..N, never off
    return true;
  }
  return false;
}

float Controller::computePowerPercent(float tempC, bool tempValid) const {
  if (_mode == Mode::Manual) {
    // Convert target voltage to the power% that FanControl will map back to it
    float v = MANUAL_VOLTS[_manualLevel - 1];
    return (v - FAN_V_MIN) / (FAN_V_MAX - FAN_V_MIN) * 100.0f;
  }
  // Auto with a broken sensor: fail safe to full power.
  if (!tempValid) return 100.0f;
  float tMax = autoRampMaxTempC(_autoLevel);
  if (tempC <= AUTO_BASE_TEMP_C) return FAN_MIN_POWER_PCT;
  if (tempC >= tMax) return 100.0f;
  float frac = (tempC - AUTO_BASE_TEMP_C) / (tMax - AUTO_BASE_TEMP_C);
  return FAN_MIN_POWER_PCT + frac * (100.0f - FAN_MIN_POWER_PCT);
}
