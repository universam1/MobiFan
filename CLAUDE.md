# MobiFan — Camper Van Fan Controller

PlatformIO (Arduino framework) firmware for an ESP32-C3 0.42" OLED board that
controls 1–2 Thermaltake Pure 20 DC fans based on a DS18B20 temperature sensor,
operated with a single button. Fan speed is set by varying the fan's supply
voltage: a CH224K USB-C PD sink (module 3C3PDSink01 / HW-443) negotiates one of
four fixed PDOs — 5/9/12/15 V — straight from the USB-C PSU, selected by the
ESP32 driving the chip's CFG1-3 pins. See
[docs/pd-sink-control.md](docs/pd-sink-control.md).

There is no boost converter, no PWM, and no analog control loop anywhere in this
firmware. (An MT3608 boost driven by FB current injection was the earlier
hardware; it was removed in `4e51d83`. Ignore any lingering reference to a
boost, an FB node, a trim pot, or `BOOST_*` constants — none of that exists.)

## Build & flash

```sh
pio run                 # build
pio run -t upload       # flash over USB
pio device monitor      # serial log, 115200 baud (USB CDC)
```

`platformio.ini` defines two envs: `esp32c3-oled-ds18b20` (the `default_envs`,
the actual firmware) and `esp32c3-oled-test`, a bench tool that builds only
`test/main.cpp` instead of `src/`.

There is no test suite and no CI. Verifying changes means building and, for
behavior, flashing to hardware and watching the serial log
(`temp=… mode=… level=… power=…% volts=…V rpm=…` printed every 2 s).

## Architecture

Single-threaded, non-blocking `millis()` loop in [src/main.cpp](src/main.cpp).
Each module is a small class with `begin()` + `tick(now)`; no delays, no RTOS
tasks, no heap use after setup.

| Module | Responsibility |
|---|---|
| `Controller` | Mode/level state machine + fan power computation (the core logic) |
| `TempSensor` / `TempSensorDS18B20` | Temperature reading, EMA smoothing; two interchangeable implementations selected by build flag (see [docs/temp-sensor.md](docs/temp-sensor.md)) |
| `FanControl` | power % → one of 4 PD voltage steps → CH224K CFG pins |
| `Tach` | Interrupt-timestamped pulse periods → RPM (2 pulses/rev) |
| `ButtonInput` | Debounce + short/long press events |
| `DisplayUi` | U8g2 rendering: main screen + change popup (see ASCII previews in DisplayUi.h) |

**All pins, temperatures, voltages, and timings live in
[src/config.h](src/config.h)** — never hardcode these in modules.

## Domain rules (do not break these)

- **Boot state**: **auto** mode, auto level `BOOT_AUTO_LEVEL` (2). Manual's level
  starts at `BOOT_MANUAL_LEVEL` (3 → 12 V) but manual is not the boot mode.
  Nothing is persisted (no NVS) — by design.
- **Both modes use levels 1..4** (`MANUAL_LEVEL_COUNT` / `AUTO_LEVEL_COUNT`).
  There is no level 0 and no level 5; short press cycles `1→4→1` in both modes,
  and each mode remembers its own level.
- **Manual mode** is a fixed voltage per level: `MANUAL_VOLTS` = 5/9/12/15 V.
  `Controller` does not command volts directly — it converts the level's voltage
  back into the power % that `FanControl` will re-quantize to the same step
  (`(v - FAN_V_MIN) / (FAN_V_MAX - FAN_V_MIN) * 100`, giving 0/40/70/100 %).
  Both halves of that round-trip must stay consistent: change `MANUAL_VOLTS`,
  `FAN_V_MIN`/`FAN_V_MAX`, or `FanControl`'s band edges and you must re-check
  that each manual level still lands on its intended step.
- **Auto mode is a ramp selector**, not a temp→level lookup: levels 1–4 pick a
  linear ramp from `FAN_MIN_POWER_PCT` (0 %) at ≤`AUTO_BASE_TEMP_C` (20 °C) to
  100 % at `autoRampMaxTempC(level)` = `47 − 7·level` → 40/33/26/19 °C.
  Auto never turns the fan fully off (0 % is the 5 V step).
  Note level 4's max (19 °C) is *below* the 20 °C base, so it degenerates into
  on/off at 20 °C. That falls out of the formula; leave the formula alone unless
  changing the ramp scheme deliberately.
- **Power → voltage**: everything outside `FanControl` deals in fan power %
  only. `FanControl::setPowerPercent()` quantizes 0–100 % onto the 4 PD steps in
  equal 25 % bands (`<25→5 V`, `<50→9 V`, `<75→12 V`, `else 15 V`) and only
  touches the CFG pins when the step actually changes. There is no continuous
  voltage control — the rail can only be one of the four PDOs.
- The main screen's bottom-right field shows the fan's **supply voltage**
  (e.g. `12.0V`), not power %. The power bar above it shows the continuous
  power %, so the bar moves within a step while the voltage does not.
- **Button**: short press cycles the level, long press ≥800 ms toggles
  manual↔auto.
- **Fail safe**: if the temp sensor reads invalid (`tempValid == false`, e.g.
  DS18B20 disconnected or NTC open/short), auto mode runs at 100 % → 15 V.
- **Stall warning**: commanded power > 0 but zero tach pulses for
  `STALL_TIMEOUT_MS` (5 s). Because the test is `power <= 0.0f`, stall detection
  is suppressed at the 5 V step (manual level 1, auto ≤20 °C) even though the
  fan does spin there — a known blind spot, inherited from when power 0 meant
  "off".
- **RPM is measured as a mean pulse period, never as a pulse count per window**:
  with 2 pulses/rev a 1 s counting window resolves only 30 RPM per pulse, so the
  reading dithered ±30 RPM. `Tach`'s ISR timestamps edges (`micros()`) and
  accumulates intervals; `tick()` averages them and applies `TACH_EMA_ALPHA`.
  `TACH_MIN_PULSE_US` is the glitch filter (the weak internal pull-up on a long
  sense wire is noise-prone, and a spurious edge now skews the interval count).
  `TACH_TIMEOUT_MS` must stay well below `STALL_TIMEOUT_MS` — it is what makes
  `rpm()` fall to 0 when the fan stops, which the stall detection relies on.
- Every level or mode change must trigger the UI popup
  (`DisplayUi::showChangePopup`).

## Hardware constraints

- **CH224K CFG pins are GPIO2/1/0** (`PIN_PD_CFG1/2/3` — note the descending
  order, CFG1 is *not* GPIO0). Driven as plain push-pull outputs with no
  external resistors: the chip has internal pull-ups and its VDD is 3.3 V from
  an internal LDO off VBUS, so levels match the C3 directly. Truth table (also
  in `FanControl::applyCfg()` and docs/pd-sink-control.md): 5 V = CFG1 HIGH
  (CFG2/3 don't care); 9 V = L,L,L; 12 V = L,L,H; 15 V = L,H,H. 20 V (L,H,L) is
  deliberately never requested — too high for the fan.
- **Boot/fail-safe state**: with the GPIOs high-Z (before `fan.begin()`, or
  firmware dead) the CH224K defaults to 5 V, the lowest step — safe. `fan.begin()`
  drives CFG for 5 V explicitly and then calls `setPowerPercent(0)`. Keeping
  `fan.begin()` first in `setup()` is cheap insurance, though unlike the old
  boost variant nothing bad happens before it runs.
- **Voltage switching is a PD renegotiation**, ~100–300 ms during which the rail
  may briefly dip. The fan's inertia covers it; do not add blocking waits or
  debouncing around `applyCfg()`. `STALL_TIMEOUT_MS` (5 s) is far longer than
  any renegotiation, so it can't false-trip.
- **A source may not offer every PDO.** The CH224K then falls back to what it
  can get, and the firmware cannot detect the difference — `fan.targetVolts()`
  and the display show the *requested* voltage, not a measured one. Don't build
  logic that assumes the rail equals the request.
- **The ESP32 board needs its own 5 V supply.** The PD output is the fan rail and
  is renegotiated up to 15 V, so it must never feed the board.
- **DS18B20 is the current hardware sensor** (env `esp32c3-oled-ds18b20`,
  the `default_envs`; **GPIO10** (`PIN_ONEWIRE`), normally powered, external
  4.7 kΩ pull-up to 3.3 V on DQ, 11-bit resolution). GPIO10 is free because the
  PD variant has no boost PWM. `TempSensorDS18B20`'s `tick()` polls a
  non-blocking conversion state machine — never make it block. It talks
  to the sensor with raw `OneWire` commands via **Skip ROM (`0xCC`)**, not
  `DallasTemperature`/address search: this specific (clone) chip has a
  genuinely invalid factory ROM CRC (confirmed by on-device debugging —
  family byte and scratchpad CRC are fine, only the ROM address CRC fails,
  a known clone-chip defect), so ROM addressing never works no matter the
  bus quality. Skip ROM sidesteps that, but only works because it's a
  single-device bus — don't add a second DS18B20 without revisiting this.
  Its `.cpp`/`.h` are wrapped in `#if defined(TEMP_SENSOR_DS18B20)` because
  PlatformIO builds every `src/*.cpp` regardless of `main.cpp`'s includes;
  see [docs/temp-sensor.md](docs/temp-sensor.md). The NTC divider
  implementation is still in the tree as a build-time alternative, but no env
  currently defines the build without `TEMP_SENSOR_DS18B20`.
- NTC divider (alternate build only): 3.3 V → NTC → GPIO3 (ADC) → 100 kΩ fixed
  → GND. The ADC reads the voltage **across the fixed resistor** (rises with
  temperature) — the conversion in TempSensor.cpp assumes this orientation.
  **Do not flip it**: the C3 ADC saturates above ~2500 mV at 11 dB attenuation;
  NTC-on-top keeps all temps < ~52 °C in the accurate window and makes an open
  NTC read ~0 V.
- **OLED panel alignment is per-board-tuned**: `OLED_X_OFFSET`/`OLED_Y_OFFSET`
  in config.h override u8g2's built-in offsets for this exact 72x40 SSD1306
  clone (whose glass window doesn't line up with u8g2's stock assumption);
  see the comment there before touching `DisplayUi`'s constructor/init.
- C3 ADC quirks: only ADC1 (GPIO0–4) is usable (ADC2 is broken/reserved on
  the C3); readings above ~2500 mV are nonlinear garbage; always read via
  `analogReadMilliVolts` so the eFuse calibration is applied.
- OLED is a 72×40 SSD1306 (I2C, SDA=5, SCL=6) using the U8g2
  `SSD1306_72X40_ER` variant — tiny canvas, every pixel of layout matters.
- The button is the board's BOOT strapping pin (GPIO9) — active low, fine at
  runtime, but holding it during reset enters the bootloader.
- WiFi and BLE are explicitly powered off in `setup()` (`WiFi.mode(WIFI_OFF)`,
  `btStop()`) — the device is a headless, battery-powered van install and never
  uses the radios.

## Conventions

- Arduino C++, no exceptions/RTTI-dependent patterns; `constexpr` config over
  `#define`.
- Modules must stay non-blocking: work is gated on `now - last >= interval`
  inside `tick()`; never call `delay()`.
- Serial output is for bring-up/debugging only — the device runs headless in a
  van; nothing may depend on a serial reader being attached.
