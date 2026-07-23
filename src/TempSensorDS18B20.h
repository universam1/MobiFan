#pragma once
#include <Arduino.h>
#include "config.h"

// Alternate TempSensor implementation for a normally-powered DS18B20 in
// place of the onboard NTC divider. Selected at build time via the
// TEMP_SENSOR_DS18B20 flag (see the esp32c3-oled-ds18b20 env in
// platformio.ini and docs/temp-sensor.md).
//
// Same public interface as TempSensor (begin/tick/celsius/valid) so main.cpp
// and every other module stay unaware of which sensor is active.
//
// Conversions are polled, never blocked on: begin() does one blocking read
// so a valid temperature exists at boot, then tick() issues a request and
// polls for completion after DS18B20_CONVERSION_MS, same as TempSensor's
// 1 Hz cadence.
//
// Talks to the sensor with raw OneWire commands using Skip ROM (0xCC)
// instead of DallasTemperature's address-based API - this specific sensor
// (a cheap clone) has a genuinely invalid factory ROM CRC (family byte and
// scratchpad both check out fine, only the 64-bit ROM address's CRC is
// bad, a known clone-chip defect), so ROM search/addressing never
// succeeds no matter the bus's electrical quality. Skip ROM sidesteps
// addressing entirely and works because this is a single-device bus (see
// docs/temp-sensor.md for the full diagnosis).
//
// Body is compiled only under TEMP_SENSOR_DS18B20 (PlatformIO builds every
// src/*.cpp regardless of which headers main.cpp includes, so the .cpp
// guards itself rather than requiring OneWire in the NTC env's lib_deps).
#if defined(TEMP_SENSOR_DS18B20)
#include <OneWire.h>

class TempSensorDS18B20 {
public:
  void begin();
  void tick(uint32_t now);
  float celsius() const { return _temp; }
  bool valid() const { return _valid; }

private:
  float readScratchpad();

  OneWire _oneWire{PIN_ONEWIRE};
  enum class State { Idle, Converting };
  State _state = State::Idle;
  float _temp = 0;
  bool _valid = false;
  uint32_t _lastRequest = 0;
  uint32_t _convertStart = 0;
};
#endif // TEMP_SENSOR_DS18B20
