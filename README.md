# MobiFan — Camper Van Fan Controller

Controls one or more Thermaltake Pure 20 DC fans from an ESP32-C3 board with a
0.42" OLED, using an NTC temperature sensor and a single button.

Fan speed is controlled by varying the fan's **supply voltage**: a D-SUN
MP1584EN buck module's output (~2.4–14 V from a 20 V input) is steered by the
ESP32 via bidirectional PWM current injection into the converter's feedback
pin. The module's onboard trim pot stays in place and is calibrated to 12.0 V
— the *anchor* the fan sees at boot or if the firmware dies — while sinking
FB current lets the firmware command up to 14 V. Only a 30 kΩ summing
resistor and a 1k/1µF RC filter are added externally — concept, schematic,
calibration, and transfer function in
[docs/buck-fb-control.md](docs/buck-fb-control.md).

## Behavior

- **Boot**: auto mode, ramp level 3. Nothing is persisted.
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

  If the NTC reads open/short, auto mode fails safe to 100%.
- Fan power maps to supply voltage: >0–100% → 4.5–14 V (`FAN_V_MIN`/`FAN_V_MAX`);
  power 0 drops the buck to 3.2 V, which stops the fan (there is no true off).
- **Display**: temperature, mode+level (`A3`/`M2`), live power bar, RPM, and
  the fan's DC output voltage.
  Any level or mode change shows a full-screen popup for 1.5 s.
  A commanded-on fan with no tach pulses for 5 s shows `! FAN STALL !`.

## Wiring

| Signal | GPIO | Notes |
|---|---|---|
| OLED SDA / SCL | 5 / 6 | onboard 72×40 SSD1306 |
| Button | 9 | onboard BOOT button, active low |
| Buck FB PWM | 10 | push-pull → 1 kΩ + 1 µF filter → 30 kΩ into MP1584EN FB |
| Fan tach | 7 | from one fan's sense wire; internal pull-up |
| NTC | 3 | divider: 3.3 V → **NTC 100k/3950** → GPIO3 → 100 kΩ → GND |

**NTC orientation matters**: the NTC sits on the high side (to 3.3 V) because
the ESP32-C3 ADC is only accurate up to ~2.5 V and saturates above. This way
the node voltage rises with temperature and stays inside the accurate range
for everything below ~52 °C, and an open sensor reads ~0 V (detectable fault
→ auto mode fails safe to 100%).

Power: 20 V DC feeds the MP1584EN, whose ~2.4–14 V output supplies the fans
(wired in parallel, tach from only one fan). The ESP32 board needs its own
fixed 5 V supply — the fan rail varies and cannot power it.

**Calibration**: with the GPIO floating (R_PWM injecting nothing), set the
module's onboard pot to **12.0 V**. That pot setting is the anchor: what the
fans see at boot (and in any firmware-dead state) — deliberately the fan's
rated voltage. The 14 V maximum is reached by the firmware sinking FB
current (~8% duty). If your measured anchor differs, adjust `BUCK_VOUT_CAL`
in [src/config.h](src/config.h).

All pins and tunables live in [src/config.h](src/config.h).

## Build

```sh
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial log (115200)
```
