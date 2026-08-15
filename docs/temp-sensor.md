# Temperature Sensor: DS18B20

MobiFan reads temperature from a single normally-powered DS18B20 on a 1-Wire
bus, implemented in [../src/TempSensorDS18B20.h](../src/TempSensorDS18B20.h) /
[../src/TempSensorDS18B20.cpp](../src/TempSensorDS18B20.cpp). It exposes
`begin()`/`tick()`/`celsius()`/`valid()`, and nothing above the sensor layer
knows anything else about it.

An NTC 100k/3950 divider used to be a second, build-flag-selectable
implementation behind that same interface (`TempSensor`, chosen by *not*
defining `TEMP_SENSOR_DS18B20`). Both the NTC code and the flag have been
removed — the DS18B20 is the hardware and the only code path. The ADC pin and
the `NTC_*` constants are gone from config.h too.

```sh
pio run                 # build (default env: esp32c3-oled-ds18b20)
pio run -t upload       # flash
```

## Why a DS18B20 and not the NTC divider

The NTC divider was cheap and needed no library, but its absolute accuracy is
limited by resistor/NTC tolerance (typically ±1–2 °C uncalibrated), and it
occupied an ADC pin with the ESP32-C3's awkward ~2.5 V accuracy ceiling. The
DS18B20 trades a little response speed for a factory-trimmed ±0.5 °C digital
reading, at the cost of one library and a third wire (normal power, not
parasite).

## Wiring (normal power, external pull-up)

Three wires run to the sensor: VDD, GND, and DQ, plus an external 4.7 kΩ
pull-up resistor from DQ to 3.3 V. Normal power was chosen over parasite
power for reliability (it avoids the strong-pull-up/bus-capacitance
concerns parasite mode has during conversion), at the cost of one extra
wire.

```
GPIO10 (PIN_ONEWIRE) -----+----- DQ  (DS18B20)
                          |
                        4.7kΩ
                          |
                        3.3V                    VDD -> 3.3V
                                                 GND -> GND
```

- `PIN_ONEWIRE` is **GPIO10** (see [../src/config.h](../src/config.h)). It became
  free when the MT3608 boost went away — GPIO10 used to carry the FB-injection
  PWM.
- With the external pull-up in place, the sensor's presence pulse and
  scratchpad reads are solid — see the ROM addressing note below for the
  one real defect found on this hardware.
- If you see intermittent `DEVICE_DISCONNECTED_C` reads (`valid() == false`,
  auto mode failing safe to 100 %), check the pull-up resistor and wiring first.

## Talks via Skip ROM, not address search — this sensor's ROM CRC is bad

`TempSensorDS18B20` doesn't use the `DallasTemperature` library or
address-based `OneWire::search()`/`getDeviceCount()` at all (and
`milesburton/DallasTemperature` was removed from the firmware env's `lib_deps`
accordingly — it survives only in the `esp32c3-oled-test` bench-tool env).
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

The `esp32c3-oled-test` env in [../platformio.ini](../platformio.ini) builds
`test/main.cpp`, a standalone "fake finder" bench tool used for that diagnosis.

## Why `tick()` polls instead of blocking

A DS18B20 conversion takes ~375 ms at the 11-bit resolution this firmware
requests (`DS18B20_RESOLUTION_BITS` in [../src/config.h](../src/config.h)
— 0.125 °C steps, plenty for fan control and faster than the default 12-bit/
750 ms), set once in `begin()` via a Skip-ROM Write Scratchpad command. Per
[../CLAUDE.md](../CLAUDE.md), no module may block the main loop, so
`TempSensorDS18B20` never blocks on a conversion from `tick()`. Instead
it's a small state machine:

1. **Idle**: issue a Skip-ROM Convert T (`0x44`) and note the start time.
2. **Converting**: once `DS18B20_CONVERSION_MS` (375 ms) has elapsed, issue
   a Skip-ROM Read Scratchpad (`0xBE`), check its CRC, and go back to Idle.

`begin()` is the one exception: it does a single blocking conversion so a
valid temperature is available immediately at boot.

Readings are EMA-smoothed with `TEMP_EMA_ALPHA` (0.2).

## Calibration

The DS18B20 is factory-calibrated to ±0.5 °C and rarely needs further
correction. If an offset is ever wanted, applying it in `readScratchpad()`
before the EMA is the natural place — nothing downstream would need to change.
