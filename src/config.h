#pragma once

// ---------- Pins (ESP32-C3 0.42" OLED board) ----------
constexpr int PIN_OLED_SDA = 5;
constexpr int PIN_OLED_SCL = 6;
constexpr int PIN_BUTTON   = 9;   // onboard BOOT button, active low
constexpr int PIN_FAN_PWM  = 10;  // open-drain, fan has internal pull-up to 5V
constexpr int PIN_FAN_TACH = 7;   // open-collector, internal pull-up
constexpr int PIN_NTC_ADC  = 3;   // ADC1_CH3, divider: 3.3V - 100k - node - NTC - GND

// ---------- NTC ----------
constexpr float NTC_R_FIXED   = 100000.0f; // series resistor to 3.3V
constexpr float NTC_R_NOMINAL = 100000.0f; // 100k at 25C
constexpr float NTC_BETA      = 3950.0f;
constexpr float NTC_T_NOMINAL = 25.0f;
constexpr float TEMP_EMA_ALPHA = 0.2f;     // smoothing, sampled at 1 Hz

// ---------- Fan ----------
constexpr uint32_t FAN_PWM_FREQ_HZ = 25000; // Intel 4-pin spec
constexpr uint8_t  FAN_PWM_RES_BITS = 10;
constexpr float    FAN_MIN_DUTY_PCT = 20.0f; // auto-mode floor (never off)
// Manual levels 0..5 -> duty %
constexpr float MANUAL_DUTY_PCT[6] = {0, 20, 40, 60, 80, 100};

// ---------- Auto mode ramps ----------
// All ramps: FAN_MIN_DUTY_PCT at <= AUTO_BASE_TEMP_C, 100% at >= rampMaxTemp(level).
// Level 1 -> 100% at 40C ... level 5 -> 100% at 20C.
constexpr float AUTO_BASE_TEMP_C = 15.0f;
constexpr float autoRampMaxTempC(uint8_t level) { return 45.0f - 5.0f * level; }

// ---------- Behavior ----------
constexpr uint8_t  BOOT_AUTO_LEVEL   = 3;    // boots into auto mode at this ramp
constexpr uint32_t LONG_PRESS_MS     = 800;
constexpr uint32_t DEBOUNCE_MS       = 30;
constexpr uint32_t POPUP_DURATION_MS = 1500;
constexpr uint32_t STALL_TIMEOUT_MS  = 5000; // duty > 0 but 0 rpm for this long
constexpr uint32_t TACH_PULSES_PER_REV = 2;
