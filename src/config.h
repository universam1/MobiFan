#pragma once

// ---------- Pins (ESP32-C3 0.42" OLED board) ----------
constexpr int PIN_OLED_SDA = 5;
constexpr int PIN_OLED_SCL = 6;

// ---------- OLED panel alignment ----------
// u8g2's U8G2_SSD1306_72X40_ER driver bakes in a 28px column offset
// (default_x_offset) tuned for the reference "EastRising" 72x40 glass.
// Cheap clone panels commonly have their visible window aligned a little
// differently over the controller's internal 128x64 RAM, so the image can
// appear cropped/shifted a few pixels on real hardware. These two knobs
// let you nudge the alignment without touching driver internals:
//   - OLED_X_OFFSET overrides u8g2's internal x_offset (columns cut off on
//     the left -> increase this in small steps; cut on the right -> decrease).
//   - OLED_Y_OFFSET is sent as the SSD1306's native "Set Display Offset"
//     command (0xD3) after init, which shifts the image vertically in
//     hardware; increasing it moves the image up on screen (content cut off
//     at the bottom / pushed down -> increase in small steps).
// Tune by flashing, watching DisplayUi::drawPopup()'s full-canvas
// drawFrame(0,0,72,40) border, and adjusting until it lines up exactly with
// the visible glass edges.
//
// This exact panel's glass window sits over COM rows 24-63 of the
// controller's 64-row RAM (vendor-documented as "x+27/y+24"), not rows
// 0-39 like the reference EastRising glass u8g2's driver assumes. Since
// the driver reprograms the multiplex ratio to 40 (driving only 40
// consecutive COM lines, u8x8_d_ssd1306_72x40.c), OLED_Y_OFFSET is sent
// via the SSD1306's native "Set Display Offset" (0xD3) register to shift
// those 40 driven COM lines onto the visible window instead of only
// partially overlapping it (which is what caused rows to appear
// missing/shifted). The exact value isn't a clean 24 in practice —
// bisected empirically on real hardware: 4 was too low (bottom row
// missing), 24 was too high (top row missing); 14 and then 13 were still
// 1px too high each time, so 12 is the settled value.
constexpr uint8_t OLED_X_OFFSET = 30; // default_x_offset (28) + small nudge
constexpr uint8_t OLED_Y_OFFSET = 12; // settled value — see note above
constexpr int PIN_BUTTON   = 9;   // onboard BOOT button, active low
constexpr int PIN_FAN_TACH = 7;   // open-collector, internal pull-up

// ---------- DS18B20 (the temperature sensor, see docs/temp-sensor.md) ----------
// Wired normally powered: VDD->3.3V, GND->GND, DQ->GPIO, with an external
// 4.7k pull-up from DQ to 3.3V.
constexpr int PIN_ONEWIRE = 10;  // GPIO10 (free since there's no boost FB PWM)
constexpr uint8_t  DS18B20_RESOLUTION_BITS = 11; // 0.125C steps, ~375ms conversion
constexpr uint32_t DS18B20_CONVERSION_MS = 375;  // 11-bit resolution conversion time
constexpr float TEMP_EMA_ALPHA = 0.2f;           // temperature smoothing

// ============================================================================
// Fan voltage control — CH224K USB-C PD sink, CFG-pin voltage selection
// ============================================================================

// ---------- CH224K USB-C PD Sink (3C3PDSink01 / HW-443) ----------
// The fan supply voltage comes directly from the USB-C PSU, negotiated by the
// CH224K via USB PD. The ESP32 selects among 4 fixed PDOs (5/9/12/15V) by
// driving the chip's CFG1-3 pins (internal pull-ups on chip; direct push-pull
// GPIO drive, no external resistors). See docs/pd-sink-control.md.
constexpr int PIN_PD_CFG1 = 2;   // HIGH = 5V; LOW = look at CFG2/3
constexpr int PIN_PD_CFG2 = 1;   // see truth table
constexpr int PIN_PD_CFG3 = 0;   // see truth table

// Available PD voltages (20V excluded — fan rated 12V, 15V is the overvoltage max)
constexpr float PD_VOLTS[] = {5.0f, 9.0f, 12.0f, 15.0f};
constexpr uint8_t PD_STEPS = 4;

constexpr float FAN_V_MIN = 5.0f;
constexpr float FAN_V_MAX = 15.0f;

// ---------- Controller ----------
constexpr uint8_t  MANUAL_LEVEL_COUNT = 4;  // levels 1..4
constexpr uint8_t  AUTO_LEVEL_COUNT   = 4;  // levels 1..4
// Manual levels 1..4 → 5V/9V/12V/15V (1-based; indexed by level-1)
constexpr float MANUAL_VOLTS[4] = {5.0f, 9.0f, 12.0f, 15.0f};
constexpr float FAN_MIN_POWER_PCT = 0.0f;   // auto floor → 5V (lowest PD step)
constexpr uint8_t BOOT_MANUAL_LEVEL = 3;    // boots at 12V (fan's rated voltage)
constexpr uint8_t BOOT_AUTO_LEVEL   = 2;

// Auto ramp: power goes from FAN_MIN_POWER_PCT at <=AUTO_BASE_TEMP_C to 100% at
// the level's end temperature. Higher level = ramps harder = full speed sooner.
// An explicit table rather than a formula: the old `47 - 7*level` put level 4's
// endpoint at 19C, i.e. *below* the base, which silently collapsed that level
// into an on/off rule at 20C instead of a ramp. Every entry must stay above
// AUTO_BASE_TEMP_C — the static_assert below enforces it.
constexpr float AUTO_BASE_TEMP_C = 20.0f;
constexpr float AUTO_RAMP_END_C[4] = {40.0f, 33.0f, 26.0f, 24.0f};
constexpr float autoRampMaxTempC(uint8_t level) { return AUTO_RAMP_END_C[level - 1]; }
static_assert(AUTO_RAMP_END_C[0] > AUTO_BASE_TEMP_C &&
              AUTO_RAMP_END_C[1] > AUTO_BASE_TEMP_C &&
              AUTO_RAMP_END_C[2] > AUTO_BASE_TEMP_C &&
              AUTO_RAMP_END_C[3] > AUTO_BASE_TEMP_C,
              "every auto ramp end temperature must be above AUTO_BASE_TEMP_C, "
              "otherwise that level degenerates into an on/off switch");

// ---------- Behavior ----------
constexpr uint32_t LONG_PRESS_MS     = 800;
constexpr uint32_t DEBOUNCE_MS       = 30;
constexpr uint32_t POPUP_DURATION_MS = 1500;
constexpr uint32_t STALL_TIMEOUT_MS  = 5000; // power > 0 but 0 rpm for this long
// Tach: RPM is derived from the mean pulse *period*, not from a pulse count per
// window. Counting pulses over 1 s quantizes to 60000/(PPR*1000) = 30 RPM per
// pulse, which made the reading dither by +-30 RPM (e.g. 840 <-> 900).
constexpr uint32_t TACH_PULSES_PER_REV = 2;
constexpr uint32_t TACH_UPDATE_MS      = 500;  // rpm recompute interval
constexpr uint32_t TACH_TIMEOUT_MS     = 1000; // no edge for this long -> 0 rpm
constexpr uint32_t TACH_MIN_PULSE_US   = 1000; // reject edges closer than this
                                               // (noise/ringing on the sense wire;
                                               // shortest real half-period is ~15 ms)
constexpr float    TACH_EMA_ALPHA      = 0.4f; // rpm smoothing, cf. TEMP_EMA_ALPHA
                                               // (tau ~1.25 s at 500 ms updates;
                                               // raise it for a snappier reading)
