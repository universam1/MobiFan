#pragma once

// ---------- Pins (ESP32-C3 0.42" OLED board) ----------
constexpr int PIN_OLED_SDA = 5;
constexpr int PIN_OLED_SCL = 6;
constexpr int PIN_BUTTON   = 9;   // onboard BOOT button, active low
constexpr int PIN_BUCK_PWM = 10;  // push-pull PWM into the MP1584EN FB filter
                                  // (see docs/buck-fb-control.md)
constexpr int PIN_FAN_TACH = 7;   // open-collector, internal pull-up
constexpr int PIN_NTC_ADC  = 3;   // ADC1_CH3 (C3: ADC1 = GPIO0-4 only, ADC2 unusable)
                                  // divider: 3.3V - NTC - node - 100k - GND
                                  // (NTC on top — see TempSensor.cpp, C3 ADC saturation)

// ---------- NTC ----------
constexpr float NTC_R_FIXED   = 100000.0f; // series resistor to GND
constexpr float NTC_R_NOMINAL = 100000.0f; // 100k at 25C
constexpr float NTC_BETA      = 3950.0f;
constexpr float NTC_T_NOMINAL = 25.0f;
constexpr float TEMP_EMA_ALPHA = 0.2f;     // smoothing, sampled at 1 Hz

// ---------- Buck converter (MP1584EN, FB current injection) ----------
// The fan (Thermaltake Pure 20, 3-pin DC) is speed-controlled by varying its
// supply voltage. The ESP32 PWMs into the buck's FB node through an RC filter
// + summing resistor; the mapping is INVERTED (high duty -> low Vout).
constexpr uint32_t BUCK_PWM_FREQ_HZ = 25000; // keep 20-50 kHz for low ripple after RC
constexpr uint8_t  BUCK_PWM_RES_BITS = 10;
constexpr float BUCK_VREF     = 0.8f;      // MP1584EN internal FB reference
constexpr float BUCK_R_TOP    = 100000.0f; // Vout -> FB
constexpr float BUCK_R_BOTTOM = 7500.0f;   // FB -> GND
constexpr float BUCK_R_PWM    = 30000.0f;  // filter output -> FB (summing)
constexpr float BUCK_LOGIC_V  = 3.3f;      // PWM high level after RC filter
constexpr float BUCK_VOUT_MIN = 3.2f;      // lowest reachable Vout (100% duty)
constexpr float BUCK_VOUT_MAX = 14.0f;     // highest reachable Vout (0% duty / boot!)

// ---------- Fan (voltage-controlled DC) ----------
constexpr float FAN_V_MIN = 4.5f;  // volts at power >0..low end (tune: must spin)
constexpr float FAN_V_MAX = 12.0f; // volts at 100% power (fan's rated voltage)
constexpr float FAN_MIN_POWER_PCT = 20.0f; // auto-mode floor (never off)
// Manual levels 0..5 -> power %  (0 = buck at BUCK_VOUT_MIN, fan stops)
constexpr float MANUAL_POWER_PCT[6] = {0, 20, 40, 60, 80, 100};

// ---------- Auto mode ramps ----------
// All ramps: FAN_MIN_POWER_PCT at <= AUTO_BASE_TEMP_C, 100% at >= rampMaxTemp(level).
// Level 1 -> 100% at 40C ... level 5 -> 100% at 20C.
constexpr float AUTO_BASE_TEMP_C = 15.0f;
constexpr float autoRampMaxTempC(uint8_t level) { return 45.0f - 5.0f * level; }

// ---------- Behavior ----------
constexpr uint8_t  BOOT_AUTO_LEVEL   = 3;    // boots into auto mode at this ramp
constexpr uint32_t LONG_PRESS_MS     = 800;
constexpr uint32_t DEBOUNCE_MS       = 30;
constexpr uint32_t POPUP_DURATION_MS = 1500;
constexpr uint32_t STALL_TIMEOUT_MS  = 5000; // power > 0 but 0 rpm for this long
constexpr uint32_t TACH_PULSES_PER_REV = 2;
