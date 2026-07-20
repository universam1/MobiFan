# MobiFan — Camper Van Fan Controller

PlatformIO (Arduino framework) firmware for an ESP32-C3 0.42" OLED board that
controls 1–2 Noctua NF-A20 5V PWM fans based on an NTC 100k/3950 temperature
sensor, operated with a single button.

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
| `Controller` | Mode/level state machine + duty computation (the core logic) |
| `TempSensor` | 1 Hz oversampled NTC read, Beta-3950 conversion, EMA smoothing |
| `FanControl` | 25 kHz LEDC PWM, open-drain output |
| `Tach` | Interrupt pulse counting → RPM (2 pulses/rev) |
| `ButtonInput` | Debounce + short/long press events |
| `DisplayUi` | U8g2 rendering: main screen + change popup (see ASCII previews in DisplayUi.h) |

**All pins, temperatures, duties, and timings live in
[src/config.h](src/config.h)** — never hardcode these in modules.

## Domain rules (do not break these)

- **Boot state**: auto mode, ramp level 3. Nothing is persisted (no NVS) — by design.
- **Auto mode is a ramp selector**, not a temp→level lookup: levels 1–5 pick a
  linear ramp from `FAN_MIN_DUTY_PCT` (20%) at ≤15 °C to 100% at 40/35/30/25/20 °C
  respectively. Auto never turns the fan fully off.
- **Manual mode**: levels 0–5 = fixed duties 0/20/40/60/80/100%.
- **Button**: short press cycles the level (manual 0→5→0, auto 1→5→1);
  long press ≥800 ms toggles manual↔auto. Manual and auto remember their
  levels independently.
- **Fail safe**: if the NTC reads open/short (`tempValid == false`), auto mode
  runs at 100%.
- **Stall warning**: duty > 0 but zero tach pulses for 5 s.
- Every level or mode change must trigger the UI popup
  (`DisplayUi::showChangePopup`).

## Hardware constraints

- Fan PWM (GPIO10) is **open-drain on purpose** — the Noctua PWM line has an
  internal pull-up to 5 V; pushing 3.3 V push-pull would fight it. Keep the
  `gpio_set_direction(..., GPIO_MODE_OUTPUT_OD)` call after `ledcAttachPin`.
- PWM must stay at 25 kHz (Intel 4-pin fan spec); audible whine below ~21 kHz.
- NTC divider: 3.3 V → 100 kΩ fixed → GPIO3 (ADC) → NTC → GND. The ADC
  reads the voltage **across the NTC** — the conversion in TempSensor.cpp
  assumes this orientation.
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
