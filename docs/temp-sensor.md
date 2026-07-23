# Temp Sensor Options: NTC Divider vs. DS18B20

MobiFan supports two interchangeable temperature sensors, selected at build
time via the PlatformIO environment. Both feed the same `Controller` /
`DisplayUi` code paths through the shared `begin()`/`tick()`/`celsius()`/
`valid()` interface — nothing above the sensor layer knows which one is
active.

| Env | Sensor | File |
|---|---|---|
| `esp32c3-oled-ds18b20` (default) | DS18B20 (1-Wire, normal power) | [../src/TempSensorDS18B20.h](../src/TempSensorDS18B20.h) / [../src/TempSensorDS18B20.cpp](../src/TempSensorDS18B20.cpp) |
| `esp32c3-oled` | NTC 100k/3950 divider | [../src/TempSensor.h](../src/TempSensor.h) / [../src/TempSensor.cpp](../src/TempSensor.cpp) |

```sh
pio run                                         # build (default env)
pio run -t upload                               # flash
```

Omitting `-e` builds `esp32c3-oled-ds18b20` (the DS18B20 env), since it's
`default_envs` in [../platformio.ini](../platformio.ini). Target
`esp32c3-oled` explicitly to build the NTC alternate.

## Why two options

The onboard NTC divider (see the main [../README.md](../README.md) and
[boost-fb-control.md](boost-fb-control.md) for the rest of the hardware) is
cheap and simple but its absolute accuracy is limited by resistor/NTC
tolerance (typically ±1–2°C uncalibrated). A DS18B20 trades a bit of
response speed for a factory-trimmed ±0.5°C digital reading, at the cost of
needing an extra library and a third wire (normal power, not parasite).

## Wiring (DS18B20, normal power, external pull-up)

Three wires run to the sensor: VDD, GND, and DQ, plus an external 4.7 kΩ
pull-up resistor from DQ to 3.3 V. Normal power was chosen over parasite
power for reliability (it avoids the strong-pull-up/bus-capacitance
concerns parasite mode has during conversion), at the cost of one extra
wire.

```
GPIO4 (PIN_ONEWIRE) ------+----- DQ  (DS18B20)
                          |
                        4.7kΩ
                          |
                        3.3V                    VDD -> 3.3V
                                                 GND -> GND
```

- `PIN_ONEWIRE` is GPIO4 (see [../src/config.h](../src/config.h)) — free in
  this project's pin map (GPIO3/5/6/7/9/10 are all already used; GPIO0–4 is
  the only usable ADC1 range on the C3, though this pin doesn't need ADC
  here, just headroom for future use).
- With the external pull-up in place, the sensor's presence pulse and
  scratchpad reads are solid — see the ROM addressing note below for the
  one real defect found on this hardware.
- If you see intermittent `DEVICE_DISCONNECTED_C` reads (`tempValid() ==
  false`, auto mode failing safe to 100%), check the pull-up resistor and
  wiring first.

## Talks via Skip ROM, not address search — this sensor's ROM CRC is bad

`TempSensorDS18B20` doesn't use the `DallasTemperature` library or
address-based `OneWire::search()`/`getDeviceCount()` at all (and
`milesburton/DallasTemperature` was removed from `lib_deps` accordingly).
It talks directly to the bus with raw commands, always addressed via
**Skip ROM (`0xCC`)**.

This is a deliberate workaround for a real hardware defect, found by active
on-device debugging: this specific (cheap/clone) DS18B20's factory-lasered
64-bit ROM address has a **genuinely invalid CRC**. `OneWire::reset()`
correctly reports a presence pulse and a raw `search()` finds a ROM with
the right family byte (`0x28`), but its CRC8 never checks out — even after
trying extra pull-up strength, the exact same (bad) ROM bytes come back
every time, ruling out a signal-integrity/timing cause. A direct Skip-ROM
read of the scratchpad, however, comes back with a **valid scratchpad CRC**
and a sane temperature — i.e. conversion and the temperature/scratchpad
CRC are fine, only the ROM CRC is bad. This is a known failure mode of
counterfeit DS18B20 chips. Since Skip ROM never needs a valid address, it
sidesteps the defect entirely — but it only works because this bus has
exactly one device; adding a second sensor would require a genuine
family-correct, CRC-valid part (or per-device wiring) to address them
individually.

## Why `tick()` polls instead of blocking

A DS18B20 conversion takes ~375 ms at the 11-bit resolution this firmware
requests (`DS18B20_RESOLUTION_BITS` in [../src/config.h](../src/config.h)
— 0.125°C steps, plenty for fan control and faster than the default 12-bit/
750 ms), set once in `begin()` via a Skip-ROM Write Scratchpad command. Per
[../CLAUDE.md](../CLAUDE.md), no module may block the main loop, so
`TempSensorDS18B20` never blocks on a conversion from `tick()`. Instead
it's a small state machine:

1. **Idle**: once per second, issue a Skip-ROM Convert T (`0x44`) and note
   the start time.
2. **Converting**: once `DS18B20_CONVERSION_MS` (375 ms) has elapsed, issue
   a Skip-ROM Read Scratchpad (`0xBE`), check its CRC, and go back to Idle.

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

