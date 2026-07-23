#include "TempSensorDS18B20.h"

#if defined(TEMP_SENSOR_DS18B20)

namespace {
constexpr uint8_t CMD_SKIP_ROM = 0xCC;
constexpr uint8_t CMD_CONVERT_T = 0x44;
constexpr uint8_t CMD_WRITE_SCRATCHPAD = 0x4E;
constexpr uint8_t CMD_READ_SCRATCHPAD = 0xBE;
constexpr float DEVICE_DISCONNECTED_C = -127.0f;

// DS18B20 config register resolution bits (datasheet Table 2): bits 6:5
// select 9/10/11/12-bit, bits 0-4 and 7 are always 1.
uint8_t resolutionConfigByte(uint8_t bits) {
  switch (bits) {
    case 9:  return 0x1F;
    case 10: return 0x3F;
    case 11: return 0x5F;
    default: return 0x7F; // 12-bit
  }
}
} // namespace

void TempSensorDS18B20::begin() {
  // Set the conversion resolution via Skip ROM (0xCC) - see the class
  // comment in TempSensorDS18B20.h for why ROM addressing is bypassed
  // entirely (this sensor's factory ROM CRC is invalid; Skip ROM works
  // because it's the only device on the bus).
  _oneWire.reset();
  _oneWire.write(CMD_SKIP_ROM);
  _oneWire.write(CMD_WRITE_SCRATCHPAD);
  _oneWire.write(0); // TH (alarm high, unused)
  _oneWire.write(0); // TL (alarm low, unused)
  _oneWire.write(resolutionConfigByte(DS18B20_RESOLUTION_BITS));

  // One blocking conversion so the UI has a valid reading immediately at
  // boot (mirrors TempSensor::begin's synchronous first read). Every
  // subsequent read goes through the non-blocking poll in tick().
  _oneWire.reset();
  _oneWire.write(CMD_SKIP_ROM);
  _oneWire.write(CMD_CONVERT_T);
  delay(DS18B20_CONVERSION_MS);
  float t = readScratchpad();

  _valid = (t > DEVICE_DISCONNECTED_C + 1.0f);
  _temp = _valid ? t : 0;
  _state = State::Idle;
}

float TempSensorDS18B20::readScratchpad() {
  _oneWire.reset();
  _oneWire.write(CMD_SKIP_ROM);
  _oneWire.write(CMD_READ_SCRATCHPAD);
  uint8_t sp[9];
  for (uint8_t &b : sp) b = _oneWire.read();
  if (OneWire::crc8(sp, 8) != sp[8]) return DEVICE_DISCONNECTED_C;
  int16_t raw = (sp[1] << 8) | sp[0];
  return raw / 16.0f;
}

void TempSensorDS18B20::tick(uint32_t now) {
  if (_state == State::Idle) {
    // Re-trigger immediately - no artificial idle gap. The only real
    // pacing limit is the sensor's own conversion time, handled below.
    _oneWire.reset();
    _oneWire.write(CMD_SKIP_ROM);
    _oneWire.write(CMD_CONVERT_T);
    _convertStart = now;
    _state = State::Converting;
    return;
  }

  // Converting: DS18B20_RESOLUTION_BITS (11-bit) needs ~375ms.
  if (now - _convertStart < DS18B20_CONVERSION_MS) return;
  float t = readScratchpad();
  _state = State::Idle;
  if (t <= DEVICE_DISCONNECTED_C + 1.0f) { _valid = false; return; }
  _temp = _valid ? _temp + TEMP_EMA_ALPHA * (t - _temp) : t;
  _valid = true;
}

#endif // TEMP_SENSOR_DS18B20
