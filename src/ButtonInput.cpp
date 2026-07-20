#include "ButtonInput.h"
#include "config.h"

void ButtonInput::begin() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
}

ButtonEvent ButtonInput::tick(uint32_t now) {
  bool raw = digitalRead(PIN_BUTTON) == LOW;
  if (raw != _rawLast) {
    _rawLast = raw;
    _rawChange = now;
  }
  if (now - _rawChange < DEBOUNCE_MS || raw == _stable) {
    if (_stable && !_longFired && now - _pressStart >= LONG_PRESS_MS) {
      _longFired = true;
      return ButtonEvent::Long;
    }
    return ButtonEvent::None;
  }
  _stable = raw;
  if (_stable) { // pressed
    _pressStart = now;
    _longFired = false;
    return ButtonEvent::None;
  }
  return _longFired ? ButtonEvent::None : ButtonEvent::Short;
}
