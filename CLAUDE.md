# MobiFan — Camper Van Fan Controller

PlatformIO (Arduino framework) firmware for an ESP32-C3 0.42" OLED board that
controls 1–2 Thermaltake Pure 20 DC fans based on an NTC 100k/3950 temperature
sensor, operated with a single button. Fan speed is set by varying the fan's
supply voltage: an MP1584EN buck converter whose output the ESP32 steers by
PWM current injection into its FB pin — see
[docs/buck-fb-control.md](docs/buck-fb-control.md).

## Build & flash

```sh
pio run                 # build
pio run -t upload       # flash over USB
pio device monitor      # serial log, 115200 baud (USB CDC)
```

There is no test suite and no CI. Verifying changes means building and, for
behavior, flashing to hardware and watching the serial log (temp/mode/level/
duty/rpm printed every 2 s).

## Architecture

Single-threaded, non-blocking `millis()` loop in [src/main.cpp](src/main.cpp).
Each module is a small class with `begin()` + `tick(now)`; no delays, no RTOS
tasks, no heap use after setup.

| Module | Responsibility |
|---|---|
| `Controller` | Mode/level state machine + fan power computation (the core logic) |
| `TempSensor` | 1 Hz oversampled NTC read, Beta-3950 conversion, EMA smoothing |
| `FanControl` | power % → target volts → inverted FB-injection PWM (25 kHz LEDC) |
| `Tach` | Interrupt pulse counting → RPM (2 pulses/rev) |
| `ButtonInput` | Debounce + short/long press events |
| `DisplayUi` | U8g2 rendering: main screen + change popup (see ASCII previews in DisplayUi.h) |

**All pins, temperatures, duties, and timings live in
[src/config.h](src/config.h)** — never hardcode these in modules.

## Domain rules (do not break these)

- **Boot state**: auto mode, ramp level 3. Nothing is persisted (no NVS) — by design.
- **Auto mode is a ramp selector**, not a temp→level lookup: levels 1–5 pick a
  linear ramp from `FAN_MIN_POWER_PCT` (20%) at ≤15 °C to 100% at 40/35/30/25/20 °C
  respectively. Auto never turns the fan fully off.
- **Manual mode**: levels 0–5 = fixed power 0/20/40/60/80/100%.
- **Power → voltage**: everything outside `FanControl` deals in fan power %
  only. `FanControl` maps power >0 linearly onto `FAN_V_MIN`..`FAN_V_MAX`
  (4.5–12 V) and power 0 to `BUCK_VOUT_MIN` (~3.2 V, fan stops — there is no
  true off).
- **Button**: short press cycles the level (manual 0→5→0, auto 1→5→1);
  long press ≥800 ms toggles manual↔auto. Manual and auto remember their
  levels independently.
- **Fail safe**: if the NTC reads open/short (`tempValid == false`), auto mode
  runs at 100%.
- **Stall warning**: duty > 0 but zero tach pulses for 5 s.
- Every level or mode change must trigger the UI popup
  (`DisplayUi::showChangePopup`).

## Hardware constraints

- The buck PWM (GPIO10) drives an RC filter into the MP1584EN FB node and is
  **inverted**: 0% duty ≈ 14 V out, 100% duty ≈ 3.1 V out. It must be
  **push-pull** (never open-drain — the pin has to source and sink), and the
  frequency must stay within 20–50 kHz so the 1k/1µF filter output is smooth.
- **Boot transient**: with the GPIO high-Z (before `fan.begin()`), the buck
  outputs ~14 V. `fan.begin()` must remain the first call in `setup()`.
- All the inversion math is contained in `FanControl::applyVolts()` — never
  spread duty-cycle inversion into other modules.
- NTC divider: 3.3 V → NTC → GPIO3 (ADC) → 100 kΩ fixed → GND. The ADC reads
  the voltage **across the fixed resistor** (rises with temperature) — the
  conversion in TempSensor.cpp assumes this orientation. **Do not flip it**:
  the C3 ADC saturates above ~2500 mV at 11 dB attenuation; NTC-on-top keeps
  all temps < ~52 °C in the accurate window and makes an open NTC read ~0 V.
- C3 ADC quirks: only ADC1 (GPIO0–4) is usable (ADC2 is broken/reserved on
  the C3); readings above ~2500 mV are nonlinear garbage; always read via
  `analogReadMilliVolts` so the eFuse calibration is applied.
- OLED is a 72×40 SSD1306 (I2C, SDA=5, SCL=6) using the U8g2
  `SSD1306_72X40_ER` variant — tiny canvas, every pixel of layout matters.
- The button is the board's BOOT strapping pin (GPIO9) — active low, fine at
  runtime, but holding it during reset enters the bootloader.
- ESP32-C3 ADC1 usable pins are GPIO0–4 only.

## Conventions

- Arduino C++, no exceptions/RTTI-dependent patterns; `constexpr` config over
  `#define`.
- Modules must stay non-blocking: work is gated on `now - last >= interval`
  inside `tick()`; never call `delay()`.
- Serial output is for bring-up/debugging only — the device runs headless in a
  van; nothing may depend on a serial reader being attached.
