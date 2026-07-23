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
constexpr int PIN_BOOST_PWM = 10; // push-pull PWM into the XL6009E1 FB filter
                                  // (see docs/boost-fb-control.md)
constexpr int PIN_FAN_TACH = 7;   // open-collector, internal pull-up
constexpr int PIN_NTC_ADC  = 3;   // ADC1_CH3 (C3: ADC1 = GPIO0-4 only, ADC2 unusable)
                                  // divider: 3.3V - NTC - node - 100k - GND
                                  // (NTC on top — see TempSensor.cpp, C3 ADC saturation)

// ---------- NTC ----------
constexpr float NTC_R_FIXED   = 100000.0f; // series resistor to GND
constexpr float NTC_R_NOMINAL = 100000.0f; // 100k at 25C
constexpr float NTC_BETA      = 3950.0f;
constexpr float NTC_T_NOMINAL = 25.0f;
constexpr float TEMP_EMA_ALPHA = 0.2f;     // smoothing, both temp sensors

// ---------- DS18B20 (current hardware sensor, see docs/temp-sensor.md) ----------
// Built when TEMP_SENSOR_DS18B20 is defined (env esp32c3-oled-ds18b20 in
// platformio.ini, the default_envs). Wired normally powered: VDD->3.3V,
// GND->GND, DQ->GPIO, with an external 4.7k pull-up from DQ to 3.3V.
// Swaps in for the NTC divider (env esp32c3-oled, kept as an alternate);
// Controller/DisplayUi are unaware which sensor is active.
#if defined(TEMP_SENSOR_DS18B20)
constexpr int PIN_ONEWIRE = 4;   // ADC1-capable pin not otherwise used
constexpr uint8_t  DS18B20_RESOLUTION_BITS = 11; // 0.125C steps, ~375ms conversion
constexpr uint32_t DS18B20_CONVERSION_MS = 375;  // 11-bit resolution conversion time
#endif

// ---------- Boost converter (XL6009E1, FB current injection) ----------
// The fan (Thermaltake Pure 20, 3-pin DC) is speed-controlled by varying its
// supply voltage. The ESP32 PWMs into the boost's FB node through an RC
// filter + summing resistor; the mapping is INVERTED (high duty -> low
// Vout) — the same FB-injection trick as a buck, topology-agnostic KCL.
// Powered from 5V USB, boosted to 5.5-14V. The module's ONBOARD 10k trim
// pot (Vout->FB) and onboard 330R pull-down are kept; only R_PWM + the RC
// filter are external. The pot is calibrated so Vout = BOOST_VOUT_CAL with
// the PWM idle/high-Z (zero injection). Injection is BIDIRECTIONAL around
// that anchor: duty above ~38% sources current into FB (Vout drops below
// the anchor), duty below ~38% sinks current (Vout rises above it). The
// anchor is deliberately the fan's rated 12 V — it is what the fan sees at
// boot and if the firmware ever dies — while the 14 V maximum is only
// reached on command (~30% duty).
constexpr uint32_t BOOST_PWM_FREQ_HZ = 25000; // keep 20-50 kHz for low ripple after RC
constexpr uint8_t  BOOST_PWM_RES_BITS = 10;
constexpr float BOOST_VREF     = 1.25f;     // XL6009E1 internal FB reference
constexpr float BOOST_VOUT_CAL = 12.0f;     // pot-set Vout at zero injection (measure!)
constexpr float BOOST_R_BOTTOM = 330.0f;    // FB -> GND (onboard on the XL6009 module)
constexpr float BOOST_R_TOP    =            // effective pot position, derived (~2.84k of
    // the pot's 10k range)
    (BOOST_VOUT_CAL - BOOST_VREF) * BOOST_R_BOTTOM / BOOST_VREF;
constexpr float BOOST_R_PWM    = 390.0f;    // filter output -> FB (summing, 1%)
constexpr float BOOST_LOGIC_V  = 3.3f;      // PWM high level after RC filter
constexpr float BOOST_VOUT_MIN = 5.5f;      // lowest commandable Vout (boost floor is
                                           // ~Vin=5V; there is no true off, see FAN_V_MIN)
constexpr float BOOST_VOUT_MAX = 14.0f;     // reached by sinking FB current (~30% duty)

// ---------- Fan (voltage-controlled DC) ----------
constexpr float FAN_V_MIN = 5.5f;  // volts at power >0..low end; equals BOOST_VOUT_MIN
                                    // since a boost can't go lower than ~Vin -- the fan
                                    // never fully stops (no true off, by design)
constexpr float FAN_V_MAX = 14.0f; // volts at 100% power
constexpr float FAN_MIN_POWER_PCT = 20.0f; // auto-mode floor (never off)
// Manual levels 0..5 -> power %  (0 = boost at BOOST_VOUT_MIN; fan keeps
// spinning at its lowest voltage, there is no true off)
constexpr float MANUAL_POWER_PCT[6] = {0, 20, 40, 60, 80, 100};

// ---------- Auto mode ramps ----------
// All ramps: FAN_MIN_POWER_PCT at <= AUTO_BASE_TEMP_C, 100% at >= rampMaxTemp(level).
// Level 1 -> 100% at 40C ... level 5 -> 100% at 20C.
constexpr float AUTO_BASE_TEMP_C = 15.0f;
constexpr float autoRampMaxTempC(uint8_t level) { return 45.0f - 5.0f * level; }

// ---------- Behavior ----------
constexpr uint8_t  BOOT_MANUAL_LEVEL = 3;    // boots into manual mode at this power step
constexpr uint8_t  BOOT_AUTO_LEVEL   = 3;    // level auto mode starts at if switched into
constexpr uint32_t LONG_PRESS_MS     = 800;
constexpr uint32_t DEBOUNCE_MS       = 30;
constexpr uint32_t POPUP_DURATION_MS = 1500;
constexpr uint32_t STALL_TIMEOUT_MS  = 5000; // power > 0 but 0 rpm for this long
constexpr uint32_t TACH_PULSES_PER_REV = 2;
