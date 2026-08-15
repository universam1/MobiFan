#include "Tach.h"
#include "config.h"

volatile uint32_t Tach::_sumUs      = 0;
volatile uint32_t Tach::_intervals  = 0;
volatile uint32_t Tach::_lastEdgeUs = 0;
volatile bool     Tach::_haveEdge   = false;

void IRAM_ATTR Tach::isr() {
  uint32_t t  = micros();
  uint32_t dt = t - _lastEdgeUs;
  // Glitch reject: too soon after the last accepted edge -> drop it without
  // re-anchoring, so ringing on one real edge cannot shift the timebase.
  if (_haveEdge && dt < TACH_MIN_PULSE_US) return;
  if (_haveEdge) {
    _sumUs += dt;
    _intervals++;
  }
  _lastEdgeUs = t;
  _haveEdge   = true;
}

void Tach::begin() {
  pinMode(PIN_FAN_TACH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), isr, FALLING);
  _lastUpdate = millis(); // else the first window is measured from boot (t=0)
}

void Tach::tick(uint32_t now) {
  if (now - _lastUpdate < TACH_UPDATE_MS) return;
  _lastUpdate = now;

  noInterrupts();
  uint32_t sumUs      = _sumUs;
  uint32_t intervals  = _intervals;
  uint32_t lastEdgeUs = _lastEdgeUs;
  bool     haveEdge   = _haveEdge;
  _sumUs     = 0;
  _intervals = 0;
  // _lastEdgeUs is deliberately kept: the interval spanning the window boundary
  // is then counted in the next window, so no interval is lost and the window
  // edge contributes no quantization error at all.
  interrupts();

  if (intervals == 0) { // no complete period in this window
    if (!haveEdge || (micros() - lastEdgeUs) > TACH_TIMEOUT_MS * 1000u) {
      noInterrupts();
      _haveEdge = false; // re-anchor on the next edge: measuring against the
                         // pre-stop timestamp would yield one huge bogus
                         // interval and a far too low first reading on restart
      interrupts();
      _rpmEma = 0.0f;
      _rpm    = 0; // stopped -> feeds the stall detection in main.cpp
    }
    return; // otherwise hold the last value (fan slower than one edge/window)
  }

  // Intervals are contiguous, so sumUs/intervals is exactly the mean period
  // over the measured span. Float keeps this off the 32-bit overflow that an
  // integer 60000000*n would hit above 72 intervals; it runs once per
  // TACH_UPDATE_MS, never in the ISR.
  float meanUs = (float)sumUs / (float)intervals;
  float rpm    = 60000000.0f / (meanUs * (float)TACH_PULSES_PER_REV);
  _rpmEma = (_rpmEma <= 0.0f) ? rpm
                              : _rpmEma + TACH_EMA_ALPHA * (rpm - _rpmEma);
  _rpm = (uint32_t)(_rpmEma + 0.5f); // round, don't truncate
}
