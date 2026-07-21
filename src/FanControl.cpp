#include "FanControl.h"
#include "config.h"

static constexpr uint8_t LEDC_CHANNEL = 0;
static constexpr uint32_t DUTY_MAX = (1u << BUCK_PWM_RES_BITS) - 1;

void FanControl::begin() {
  ledcSetup(LEDC_CHANNEL, BUCK_PWM_FREQ_HZ, BUCK_PWM_RES_BITS);
  ledcAttachPin(PIN_BUCK_PWM, LEDC_CHANNEL);
  // Push-pull (default) on purpose: the pin drives the 1k/1uF RC filter and
  // must both source and sink current into the FB summing resistor.
  setPowerPercent(0);
}

void FanControl::setPowerPercent(float pct) {
  _power = constrain(pct, 0.0f, 100.0f);
  if (_power <= 0.0f) {
    applyVolts(BUCK_VOUT_MIN); // no true off — lowest voltage stops the fan
    return;
  }
  applyVolts(FAN_V_MIN + _power / 100.0f * (FAN_V_MAX - FAN_V_MIN));
}

// KCL at the FB node (held at BUCK_VREF by the regulator):
//   Vout = Vref + Rtop * (Vref/Rbottom - (Vpwm - Vref)/Rpwm)
// solved for the filtered PWM voltage needed for a target Vout:
//   Vpwm = Vref + Rpwm * (Vref/Rbottom - (Vout - Vref)/Rtop)
// Note the inversion: higher duty -> higher Vpwm -> LOWER Vout.
void FanControl::applyVolts(float v) {
  _volts = constrain(v, BUCK_VOUT_MIN, BUCK_VOUT_MAX);
  float vPwm = BUCK_VREF + BUCK_R_PWM * (BUCK_VREF / BUCK_R_BOTTOM -
                                         (_volts - BUCK_VREF) / BUCK_R_TOP);
  float duty = constrain(vPwm / BUCK_LOGIC_V, 0.0f, 1.0f);
  ledcWrite(LEDC_CHANNEL, (uint32_t)(duty * DUTY_MAX + 0.5f));
}
