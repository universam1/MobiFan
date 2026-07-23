# MobiFan — Camper Van Fan Controller

Controls one or more Thermaltake Pure 20 DC fans from an ESP32-C3 board with a
0.42" OLED, using a DS18B20 (or optional NTC) temperature sensor and a
single button.

Fan speed is controlled by varying the fan's **supply voltage**: an XL6009E1
boost module's output (~5.5–14 V from a 5 V USB input) is steered by the
ESP32 via bidirectional PWM current injection into the converter's feedback
pin. The module's onboard 10 kΩ trim pot stays in place and is calibrated to
12.0 V — the *anchor* the fan sees at boot or if the firmware dies — while
sinking FB current lets the firmware command up to 14 V. Only a 390 Ω summing
resistor and a 330Ω/4.7µF RC filter are added externally — concept, schematic,
calibration, and transfer function in
[docs/boost-fb-control.md](docs/boost-fb-control.md).

## Behavior

- **Boot**: manual mode, level 3. Nothing is persisted.
- **Short press**: manual mode cycles level 0→5→0 (fixed power 0/20/40/60/80/100%);
  auto mode cycles ramp 1→5→1 (auto never turns the fan off).
- **Long press (≥0.8 s)**: toggles manual ↔ auto.
- **Auto mode**: fan power ramps linearly from the minimum (20%) at ≤15 °C up to
  100% at the selected ramp's max temperature:

  | Auto level | 100% at |
  |---|---|
  | 1 | 40 °C |
  | 2 | 35 °C |
  | 3 | 30 °C |
  | 4 | 25 °C |
  | 5 | 20 °C |

  If the temp sensor reads invalid, auto mode fails safe to 100%.
- Fan power maps to supply voltage: >0–100% → 5.5–14 V (`FAN_V_MIN`/`FAN_V_MAX`);
  power 0 drops the boost to the same 5.5 V floor (`BOOST_VOUT_MIN`) — a
  boost can't go lower than its own ~5 V input, so the fan never fully stops
  (there is no true off).
- **Display**: temperature, mode+level (`A3`/`M2`), live power bar, RPM, and
  the fan's DC output voltage.
  Any level or mode change shows a full-screen popup for 1.5 s.
  A commanded-on fan with no tach pulses for 5 s shows `! FAN STALL !`.

## Wiring

| Signal | GPIO | Notes |
|---|---|---|
| OLED SDA / SCL | 5 / 6 | onboard 72×40 SSD1306 |
| Button | 9 | onboard BOOT button, active low |
| Boost FB PWM | 10 | push-pull → 330 Ω + 4.7 µF filter → 390 Ω into XL6009E1 FB |
| Fan tach | 7 | from one fan's sense wire; internal pull-up |
| DS18B20 | 4 | 1-Wire, normally powered, external 4.7 kΩ pull-up to 3.3 V |
| NTC (optional alt.) | 3 | divider: 3.3 V → **NTC 100k/3950** → GPIO3 → 100 kΩ → GND |

**NTC orientation matters**: the NTC sits on the high side (to 3.3 V) because
the ESP32-C3 ADC is only accurate up to ~2.5 V and saturates above. This way
the node voltage rises with temperature and stays inside the accurate range
for everything below ~52 °C, and an open sensor reads ~0 V (detectable fault
→ auto mode fails safe to 100%).

**DS18B20 is the default sensor** (env `esp32c3-oled-ds18b20`, PlatformIO's
`default_envs`), wired with an external 4.7 kΩ pull-up from DQ to 3.3 V. The
plain NTC divider (env `esp32c3-oled`) remains available as a build-time
alternative — both sensors share the same interface so the rest of the
firmware is unchanged either way. Wiring, rationale, and the non-blocking
conversion polling are covered in [docs/temp-sensor.md](docs/temp-sensor.md).

Power: 5 V USB feeds the XL6009E1, whose ~5.5–14 V output supplies the fans
(wired in parallel, tach from only one fan). The ESP32 board needs its own
fixed 5 V supply — the fan rail varies and cannot power it.

**Calibration**: with the GPIO floating (R_PWM injecting nothing), set the
module's onboard 10 kΩ pot to **12.0 V**. That pot setting is the anchor:
what the fans see at boot (and in any firmware-dead state) — deliberately
the fan's rated voltage. The 14 V maximum is reached by the firmware sinking
FB current (~30% duty). If your measured anchor differs, adjust
`BOOST_VOUT_CAL` in [src/config.h](src/config.h).

All pins and tunables live in [src/config.h](src/config.h).

## Build

```sh
pio run                 # build (default env: DS18B20 sensor)
pio run -t upload       # flash
pio device monitor      # serial log (115200)
```

To build with the plain NTC divider instead, target its env explicitly (see
[docs/temp-sensor.md](docs/temp-sensor.md)):

```sh
pio run -e esp32c3-oled
pio run -e esp32c3-oled -t upload
```
