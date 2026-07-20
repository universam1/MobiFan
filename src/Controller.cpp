#include "Controller.h"
#include "config.h"

bool Controller::handleButton(ButtonEvent ev) {
  if (ev == ButtonEvent::Long) {
    _mode = (_mode == Mode::Auto) ? Mode::Manual : Mode::Auto;
    return true;
  }
  if (ev == ButtonEvent::Short) {
    if (_mode == Mode::Manual)
      _manualLevel = (_manualLevel + 1) % 6;        // 0..5, wraps to off
    else
      _autoLevel = _autoLevel % 5 + 1;              // 1..5, never off
    return true;
  }
  return false;
}

float Controller::computeDutyPercent(float tempC, bool tempValid) const {
  if (_mode == Mode::Manual) return MANUAL_DUTY_PCT[_manualLevel];
  // Auto with a broken sensor: fail safe to full power.
  if (!tempValid) return 100.0f;
  float tMax = autoRampMaxTempC(_autoLevel);
  if (tempC <= AUTO_BASE_TEMP_C) return FAN_MIN_DUTY_PCT;
  if (tempC >= tMax) return 100.0f;
  float frac = (tempC - AUTO_BASE_TEMP_C) / (tMax - AUTO_BASE_TEMP_C);
  return FAN_MIN_DUTY_PCT + frac * (100.0f - FAN_MIN_DUTY_PCT);
}
