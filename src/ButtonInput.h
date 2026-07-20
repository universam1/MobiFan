#pragma once
#include <Arduino.h>

enum class ButtonEvent { None, Short, Long };

// Debounced active-low button. Short press fires on release; long press
// fires once while still held (release is then ignored).
class ButtonInput {
public:
  void begin();
  ButtonEvent tick(uint32_t now);

private:
  bool _stable = false;      // debounced pressed state
  bool _rawLast = false;
  bool _longFired = false;
  uint32_t _rawChange = 0;
  uint32_t _pressStart = 0;
};
