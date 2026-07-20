#include "Tach.h"
#include "config.h"

volatile uint32_t Tach::_pulses = 0;

void IRAM_ATTR Tach::isr() { _pulses++; }

void Tach::begin() {
  pinMode(PIN_FAN_TACH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), isr, FALLING);
}

void Tach::tick(uint32_t now) {
  if (now - _lastWindow < 1000) return;
  uint32_t elapsed = now - _lastWindow;
  _lastWindow = now;
  noInterrupts();
  uint32_t pulses = _pulses;
  _pulses = 0;
  interrupts();
  _rpm = pulses * 60000u / (TACH_PULSES_PER_REV * elapsed);
}
