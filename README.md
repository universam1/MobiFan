# MobiFan — Camper Van Fan Controller

Controls one or more Thermaltake Pure 20 DC fans from an ESP32-C3 board with a
0.42" OLED, using a DS18B20 temperature sensor and a single button.

Fan speed is controlled by varying the fan's **supply voltage**. A CH224K USB-C
PD sink (module: 3C3PDSink01 / HW-443) negotiates the voltage straight from the
USB-C power supply; the ESP32 selects among four fixed PDOs — **5 / 9 / 12 / 15 V**
— by driving the chip's CFG1-3 pins. There is no boost converter and no analog
control loop: the fan rail is whatever PDO is currently selected. Concept,
truth table, and wiring in [docs/pd-sink-control.md](docs/pd-sink-control.md).

The fan is rated 12 V; 15 V is a deliberate slight overvoltage for extra
airflow, and at 5 V the fan still turns slowly — **there is no true off**.

## Behavior

- **Boot**: auto mode, level 2. Nothing is persisted (no NVS).
- **Short press**: cycles the level `1→2→3→4→1` — in both modes, and both modes
  remember their own level independently.
- **Long press (≥0.8 s)**: toggles manual ↔ auto.
- **Manual mode**: level = a fixed voltage.

  | Manual level | Fan voltage |
  |---|---|
  | 1 | 5 V |
  | 2 | 9 V |
  | 3 | 12 V (fan's rated voltage) |
  | 4 | 15 V |

- **Auto mode**: the level selects a *ramp*, not a voltage. Fan power ramps
  linearly from 0 % at ≤20 °C (`AUTO_BASE_TEMP_C`) to 100 % at the ramp's max
  temperature, and that continuous power is then quantized onto the four PD
  steps. Auto never turns the fan off — 0 % is the 5 V step.

  | Auto level | 100 % at | 5 V below | 9 V | 12 V | 15 V above |
  |---|---|---|---|---|---|
  | 1 | 40 °C | 25 °C | 25–30 °C | 30–35 °C | 35 °C |
  | 2 | 33 °C | 23.3 °C | 23.3–26.5 °C | 26.5–29.8 °C | 29.8 °C |
  | 3 | 26 °C | 21.5 °C | 21.5–23 °C | 23–24.5 °C | 24.5 °C |
  | 4 | 19 °C | — | — | — | 20 °C |

  Auto level 4's ramp max (19 °C) is *below* the 20 °C base, so it collapses
  into an on/off rule: 5 V at ≤20 °C, 15 V above it. The ramp maxima come from
  `autoRampMaxTempC()` = `47 − 7 × level` in [src/config.h](src/config.h).

  If the temp sensor reads invalid, auto mode fails safe to 100 % (15 V).
- **Display**: temperature, mode+level (`A2`/`M3`), live power bar, RPM, and the
  fan's supply voltage. Any level or mode change shows a full-screen popup for
  1.5 s. A commanded-on fan with no tach pulses for 5 s shows `FAN STALL!`.
  Note that stall detection is suppressed while commanded power is 0 % (manual
  level 1, and auto at ≤20 °C) even though the fan does spin at 5 V there.

## Wiring

| Signal | GPIO | Notes |
|---|---|---|
| OLED SDA / SCL | 5 / 6 | onboard 72×40 SSD1306 |
| Button | 9 | onboard BOOT button, active low |
| PD CFG1 / CFG2 / CFG3 | 2 / 1 / 0 | direct to CH224K, no external resistors |
| Fan tach | 7 | from one fan's sense wire; internal pull-up |
| DS18B20 | 10 | 1-Wire, normally powered, external 4.7 kΩ pull-up to 3.3 V |
| NTC (alternate build) | 3 | divider: 3.3 V → **NTC 100k/3950** → GPIO3 → 100 kΩ → GND |

Power: a USB-C PD supply feeds the CH224K module, whose output (5–15 V) goes to
the fans — wired in parallel, tach taken from one fan only. **The ESP32 board
needs its own fixed 5 V supply**: the fan rail is renegotiated up to 15 V and
cannot feed the board.

**DS18B20 is the sensor the firmware ships with** (env `esp32c3-oled-ds18b20`,
PlatformIO's `default_envs`), wired with an external 4.7 kΩ pull-up from DQ to
3.3 V. An NTC divider implementation ([src/TempSensor.cpp](src/TempSensor.cpp))
is still in the tree behind the same interface, but no PlatformIO env currently
builds it — see [docs/temp-sensor.md](docs/temp-sensor.md).

**NTC orientation matters** (for that alternate build): the NTC sits on the high
side (to 3.3 V) because the ESP32-C3 ADC is only accurate up to ~2.5 V and
saturates above. This way the node voltage rises with temperature and stays
inside the accurate range for everything below ~52 °C, and an open sensor reads
~0 V (detectable fault → auto mode fails safe to 100 %).

All pins and tunables live in [src/config.h](src/config.h) — nothing is
hardcoded in the modules.

## Build

```sh
pio run                 # build (default env: esp32c3-oled-ds18b20)
pio run -t upload       # flash
pio device monitor      # serial log (115200)
```
