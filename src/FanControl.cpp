#include "FanControl.h"
#include "config.h"

void FanControl::begin() {
  pinMode(PIN_PD_CFG1, OUTPUT);
  pinMode(PIN_PD_CFG2, OUTPUT);
  pinMode(PIN_PD_CFG3, OUTPUT);
  // Start at 5V (safe default, matches CH224K's own power-on state when
  // CFG1 is floating/HIGH).
  digitalWrite(PIN_PD_CFG1, HIGH);
  digitalWrite(PIN_PD_CFG2, HIGH);
  digitalWrite(PIN_PD_CFG3, HIGH);
  setPowerPercent(0);
}

void FanControl::setPowerPercent(float pct) {
  _power = constrain(pct, 0.0f, 100.0f);

  // Quantize continuous power% to the 4 discrete PD voltage steps.
  // Equal-width 25% bands so all 4 steps (including 5V) are reachable:
  //   0..24%   -> 5V
  //   25..49%  -> 9V
  //   50..74%  -> 12V  (fan's rated voltage)
  //   75..100% -> 15V
  float volts;
  if (_power < 25.0f)      volts = PD_VOLTS[0];  // 5V
  else if (_power < 50.0f) volts = PD_VOLTS[1];  // 9V
  else if (_power < 75.0f) volts = PD_VOLTS[2];  // 12V
  else                     volts = PD_VOLTS[3];  // 15V

  if (volts != _volts) {
    _volts = volts;
    applyCfg(_volts);
  }
}

// Drive CH224K CFG1-3 per its truth table:
//   5V:  CFG1=HIGH (CFG2/3 don't care)
//   9V:  CFG1=LOW, CFG2=LOW,  CFG3=LOW
//   12V: CFG1=LOW, CFG2=LOW,  CFG3=HIGH
//   15V: CFG1=LOW, CFG2=HIGH, CFG3=HIGH
void FanControl::applyCfg(float volts) {
  if (volts <= 5.0f) {
    digitalWrite(PIN_PD_CFG1, HIGH);
    digitalWrite(PIN_PD_CFG2, HIGH);
    digitalWrite(PIN_PD_CFG3, HIGH);
  } else if (volts <= 9.0f) {
    digitalWrite(PIN_PD_CFG1, LOW);
    digitalWrite(PIN_PD_CFG2, LOW);
    digitalWrite(PIN_PD_CFG3, LOW);
  } else if (volts <= 12.0f) {
    digitalWrite(PIN_PD_CFG1, LOW);
    digitalWrite(PIN_PD_CFG2, LOW);
    digitalWrite(PIN_PD_CFG3, HIGH);
  } else {
    // 15V
    digitalWrite(PIN_PD_CFG1, LOW);
    digitalWrite(PIN_PD_CFG2, HIGH);
    digitalWrite(PIN_PD_CFG3, HIGH);
  }
}
