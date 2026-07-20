# MobiFan — Camper Van Fan Controller

Controls one or more (Noctua NF-A20 5V PWM) fans from an ESP32-C3 board with a
0.42" OLED, using an NTC temperature sensor and a single button.

## Behavior

- **Boot**: auto mode, ramp level 3. Nothing is persisted.
- **Short press**: manual mode cycles level 0→5→0 (fixed duties 0/20/40/60/80/100%);
  auto mode cycles ramp 1→5→1 (auto never turns the fan off).
- **Long press (≥0.8 s)**: toggles manual ↔ auto.
- **Auto mode**: fan duty ramps linearly from the minimum (20%) at ≤15 °C up to
  100% at the selected ramp's max temperature:

  | Auto level | 100% at |
  |---|---|
  | 1 | 40 °C |
  | 2 | 35 °C |
  | 3 | 30 °C |
  | 4 | 25 °C |
  | 5 | 20 °C |

  If the NTC reads open/short, auto mode fails safe to 100%.
- **Display**: temperature, mode+level (`A3`/`M2`), live duty bar, RPM.
  Any level or mode change shows a full-screen popup for 1.5 s.
  A commanded-on fan with no tach pulses for 5 s shows `! FAN STALL !`.

## Wiring

| Signal | GPIO | Notes |
|---|---|---|
| OLED SDA / SCL | 5 / 6 | onboard 72×40 SSD1306 |
| Button | 9 | onboard BOOT button, active low |
| Fan PWM (blue) | 10 | open-drain; fan pulls up to 5 V internally — no level shifter |
| Fan tach (green) | 7 | from one fan; internal pull-up |
| NTC | 3 | divider: 3.3 V → **NTC 100k/3950** → GPIO3 → 100 kΩ → GND |

**NTC orientation matters**: the NTC sits on the high side (to 3.3 V) because
the ESP32-C3 ADC is only accurate up to ~2.5 V and saturates above. This way
the node voltage rises with temperature and stays inside the accurate range
for everything below ~52 °C, and an open sensor reads ~0 V (detectable fault
→ auto mode fails safe to 100%).

Power: 12 V → 5 V buck feeds the fans and the board's 5 V pin. Fan PWM and
tach lines are shared/parallel for two fans (tach from only one fan).

All pins and tunables live in [src/config.h](src/config.h).

## Build

```sh
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial log (115200)
```
