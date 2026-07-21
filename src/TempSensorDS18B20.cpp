#include "TempSensorDS18B20.h"

#if defined(TEMP_SENSOR_DS18B20)

void TempSensorDS18B20::begin() {
  _sensors.begin();
  _sensors.setResolution(DS18B20_RESOLUTION_BITS);

  // No external pull-up resistor: rely on the C3's internal weak pull-up
  // instead (see docs/temp-sensor.md). OneWire::begin() (called from the
  // _oneWire constructor, above) leaves the pin as plain INPUT, so this has
  // to be set *after* that. On ESP32-C3, OneWire's read/write/reset only
  // ever toggle the GPIO output-enable bit (GPIO.enable_w1tc/w1ts) and never
  // touch the pull configuration, so this sticks for the module's lifetime.
  pinMode(PIN_ONEWIRE, INPUT_PULLUP);

  // One blocking conversion so the UI has a valid reading immediately at
  // boot (mirrors TempSensor::begin's synchronous first read). Every
  // subsequent read goes through the non-blocking poll in tick().
  _sensors.setWaitForConversion(true);
  _sensors.requestTemperatures();
  float t = _sensors.getTempCByIndex(0);
  _sensors.setWaitForConversion(false);

  _valid = (t > DEVICE_DISCONNECTED_C + 1.0f);
  _temp = _valid ? t : 0;
  _lastRequest = millis();
  _state = State::Idle;
}

void TempSensorDS18B20::tick(uint32_t now) {
  if (_state == State::Idle) {
    if (now - _lastRequest < 1000) return;
    _sensors.requestTemperatures();
    _convertStart = now;
    _state = State::Converting;
    return;
  }

  // Converting: DS18B20_RESOLUTION_BITS (11-bit) needs ~375ms.
  if (now - _convertStart < DS18B20_CONVERSION_MS) return;
  float t = _sensors.getTempCByIndex(0);
  _lastRequest = now;
  _state = State::Idle;
  if (t <= DEVICE_DISCONNECTED_C + 1.0f) { _valid = false; return; }
  _temp = _valid ? _temp + TEMP_EMA_ALPHA * (t - _temp) : t;
  _valid = true;
}

#endif // TEMP_SENSOR_DS18B20
