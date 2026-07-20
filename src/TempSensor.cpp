#include "TempSensor.h"
#include "config.h"

void TempSensor::begin() {
  analogSetPinAttenuation(PIN_NTC_ADC, ADC_11db);
  _temp = readOnce();
  _valid = !isnan(_temp);
}

void TempSensor::tick(uint32_t now) {
  if (now - _lastSample < 1000) return;
  _lastSample = now;
  float t = readOnce();
  if (isnan(t)) { _valid = false; return; }
  _temp = _valid ? _temp + TEMP_EMA_ALPHA * (t - _temp) : t;
  _valid = true;
}

float TempSensor::readOnce() const {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_NTC_ADC);
  mv /= 8;
  // Divider: 3.3V - R_FIXED - node - NTC - GND, so mv is across the NTC.
  if (mv < 20 || mv > 3280) return NAN; // open/short sensor
  float rNtc = NTC_R_FIXED * (float)mv / (3300.0f - (float)mv);
  float tKelvin = 1.0f / (1.0f / (NTC_T_NOMINAL + 273.15f) +
                          logf(rNtc / NTC_R_NOMINAL) / NTC_BETA);
  return tKelvin - 273.15f;
}
