#include "FanControl.h"
#include "config.h"

static constexpr uint8_t LEDC_CHANNEL = 0;
static constexpr uint32_t DUTY_MAX = (1u << BOOST_PWM_RES_BITS) - 1;

void FanControl::begin() {
  ledcSetup(LEDC_CHANNEL, BOOST_PWM_FREQ_HZ, BOOST_PWM_RES_BITS);
  ledcAttachPin(PIN_BOOST_PWM, LEDC_CHANNEL);
  // Push-pull (default) on purpose: the pin drives the 330R/4.7uF RC filter
  // and must both source and sink current into the FB summing resistor.
  setPowerPercent(0);
}

void FanControl::setPowerPercent(float pct) {
  _power = constrain(pct, 0.0f, 100.0f);
  if (_power <= 0.0f) {
    applyVolts(BOOST_VOUT_MIN); // no true off — lowest voltage the fan sees
    return;
  }
  applyVolts(FAN_V_MIN + _power / 100.0f * (FAN_V_MAX - FAN_V_MIN));
}

// KCL at the FB node (held at BOOST_VREF by the regulator):
//   Vout = Vref + Rtop * (Vref/Rbottom - (Vpwm - Vref)/Rpwm)
// solved for the filtered PWM voltage needed for a target Vout:
//   Vpwm = Vref + Rpwm * (Vref/Rbottom - (Vout - Vref)/Rtop)
// Note the inversion: higher duty -> higher Vpwm -> LOWER Vout. This KCL is
// topology-agnostic (buck or boost), only the constants differ.
void FanControl::applyVolts(float v) {
  _volts = constrain(v, BOOST_VOUT_MIN, BOOST_VOUT_MAX);
  float vPwm = BOOST_VREF + BOOST_R_PWM * (BOOST_VREF / BOOST_R_BOTTOM -
                                           (_volts - BOOST_VREF) / BOOST_R_TOP);
  float duty = constrain(vPwm / BOOST_LOGIC_V, 0.0f, 1.0f);
  ledcWrite(LEDC_CHANNEL, (uint32_t)(duty * DUTY_MAX + 0.5f));
}
