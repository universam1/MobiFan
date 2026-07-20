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
  // Divider: 3.3V - NTC - node - R_FIXED - GND, so mv is across the fixed
  // resistor and rises with temperature. NTC on the high side on purpose:
  // the ESP32-C3 ADC is only accurate up to ~2500 mV at 11 dB attenuation
  // and saturates above — this orientation keeps every temperature below
  // ~52C (node < 2.5 V) inside the accurate window, and clipping only
  // occurs where any auto ramp is at 100% anyway.
  if (mv < 50) return NAN;   // open NTC (a real read this low would mean < -60C)
  if (mv > 3200) return NAN; // shorted NTC
  float rNtc = NTC_R_FIXED * (3300.0f - (float)mv) / (float)mv;
  float tKelvin = 1.0f / (1.0f / (NTC_T_NOMINAL + 273.15f) +
                          logf(rNtc / NTC_R_NOMINAL) / NTC_BETA);
  return tKelvin - 273.15f;
}
