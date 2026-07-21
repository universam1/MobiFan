# Temp Sensor Options: NTC Divider vs. DS18B20

MobiFan supports two interchangeable temperature sensors, selected at build
time via the PlatformIO environment. Both feed the same `Controller` /
`DisplayUi` code paths through the shared `begin()`/`tick()`/`celsius()`/
`valid()` interface — nothing above the sensor layer knows which one is
active.

| Env | Sensor | File |
|---|---|---|
| `esp32c3-oled` (default) | NTC 100k/3950 divider | [../src/TempSensor.h](../src/TempSensor.h) / [../src/TempSensor.cpp](../src/TempSensor.cpp) |
| `esp32c3-oled-ds18b20` | DS18B20 (1-Wire, normal power) | [../src/TempSensorDS18B20.h](../src/TempSensorDS18B20.h) / [../src/TempSensorDS18B20.cpp](../src/TempSensorDS18B20.cpp) |

```sh
pio run -e esp32c3-oled-ds18b20                 # build
pio run -e esp32c3-oled-ds18b20 -t upload       # flash
```

Omitting `-e` builds `esp32c3-oled` (the NTC env), since it's
`default_envs` in [../platformio.ini](../platformio.ini).

## Why two options

The onboard NTC divider (see the main [../README.md](../README.md) and
[buck-fb-control.md](buck-fb-control.md) for the rest of the hardware) is
cheap and simple but its absolute accuracy is limited by resistor/NTC
tolerance (typically ±1–2°C uncalibrated). A DS18B20 trades a bit of
response speed for a factory-trimmed ±0.5°C digital reading, at the cost of
needing an extra library and a third wire (normal power, not parasite).

## Wiring (DS18B20, normal power, no pull-up resistor)

Three wires run to the sensor: VDD, GND, and DQ — but no external pull-up.
Normal power was chosen over parasite power for reliability (it avoids the
strong-pull-up/bus-capacitance concerns parasite mode has during
conversion), at the cost of one extra wire. The pull-up resistor is dropped
in favor of the ESP32-C3's internal weak pull-up, following the technique
from [bigjosh's "No external pull-up needed for DS18B20"
writeup](https://wp.josh.com/2014/06/23/no-external-pull-up-needed-for-ds18b20-temp-sensor/)
(and its [ESP8266 fork](https://github.com/bigjosh/OneWireNoResistor/tree/ESP8266)),
adapted here without forking the OneWire library.

```
GPIO4 (PIN_ONEWIRE) ------ DQ  (DS18B20)
                            VDD -> 3.3V
                            GND -> GND
```

- `PIN_ONEWIRE` is GPIO4 (see [../src/config.h](../src/config.h)) — free in
  this project's pin map (GPIO3/5/6/7/9/10 are all already used; GPIO0–4 is
  the only usable ADC1 range on the C3, though this pin doesn't need ADC
  here, just headroom for future use).
- `TempSensorDS18B20::begin()` calls `pinMode(PIN_ONEWIRE, INPUT_PULLUP)`
  once, right after `OneWire`/`DallasTemperature` init (it has to come
  *after*, since `OneWire::begin()` itself sets plain `INPUT`). This was
  verified against this project's vendored `OneWire` library: on
  `CONFIG_IDF_TARGET_ESP32C3` its `directModeInput`/`directModeOutput`
  (used by every `reset()`/`read_bit()`/`write_bit()`) only toggle
  `GPIO.enable_w1tc`/`w1ts` (the output-enable bit) and never touch the
  pull-up/pull-down registers — so the pull-up set once at boot holds for
  every future bus transaction without needing a patched library.
- This relies on a single sensor and a short cable, matching the writeup's
  own caveats: the internal pull-up is much weaker (tens of kΩ vs. 4.7 kΩ),
  so its RC rise time budget is smaller. Longer wiring or multiple sensors
  would eat that margin and are more likely to need the external resistor
  back.
- If you see intermittent `DEVICE_DISCONNECTED_C` reads (`tempValid() ==
  false`, auto mode failing safe to 100%), that's the first thing to
  suspect. A 4.7 kΩ external pull-up to 3.3 V (added in parallel, doesn't
  need code changes) is the fallback if the internal one isn't reliable
  enough for your wiring.


## Why `tick()` polls instead of blocking

A DS18B20 conversion takes ~375 ms at the 11-bit resolution this firmware
requests (`DS18B20_RESOLUTION_BITS` in [../src/config.h](../src/config.h)
— 0.125°C steps, plenty for fan control and faster than the default 12-bit/
750 ms). Per [../CLAUDE.md](../CLAUDE.md), no module may block the main
loop, so `TempSensorDS18B20` never calls the library's blocking
`requestTemperatures()` from `tick()`. Instead it's a small state machine:

1. **Idle**: once per second, issue `requestTemperatures()` (non-blocking —
   `setWaitForConversion(false)`) and note the start time.
2. **Converting**: once `DS18B20_CONVERSION_MS` (375 ms) has elapsed, read
   the result and go back to Idle.

`begin()` is the one exception: it does a single blocking conversion so a
valid temperature is available immediately at boot, mirroring
`TempSensor::begin()`'s synchronous first NTC read.

## Build system note

PlatformIO compiles every `.cpp` under `src/` regardless of which headers
`main.cpp` includes, so `TempSensorDS18B20.cpp`/`.h` wrap their bodies in
`#if defined(TEMP_SENSOR_DS18B20)` — otherwise the NTC env would fail to
link against `OneWire`/`DallasTemperature`, which aren't in its `lib_deps`.
`main.cpp` picks the class to instantiate with the same macro.

## Calibrating either sensor

Both sensors read through the same `celsius()`/`valid()` interface. The
DS18B20 is factory-calibrated to ±0.5°C and rarely needs further correction.
The NTC benefits more from a one-point offset check against a reference
thermometer — that isn't implemented in firmware today, but since both
sensors return through the same interface, an offset constant applied in
`TempSensor::readOnce()` would be the natural place to add it if needed.

