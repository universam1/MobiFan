#include "FanControl.h"
#include "config.h"

static constexpr uint8_t LEDC_CHANNEL = 0;
static constexpr uint32_t DUTY_MAX = (1u << BOOST_PWM_RES_BITS) - 1;

void FanControl::begin() {
  ledcSetup(LEDC_CHANNEL, BOOST_PWM_FREQ_HZ, BOOST_PWM_RES_BITS);
  ledcAttachPin(PIN_BOOST_PWM, LEDC_CHANNEL);
  // Push-pull (default) on purpose: the pin drives the 1k/2.2uF RC filter
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
//   Vout = Vref + Rtop * (Vref/Rbottom - (Vnode - Vref)/Rpwm)
// solved for the filter-output voltage needed for a target Vout:
//   Vnode = Vref + Rpwm * (Vref/Rbottom - (Vout - Vref)/Rtop)
// Vnode is not the GPIO's average voltage: R_FILT and R_PWM form a divider
// between the pin and the FB node, so the pin must overdrive by the drop
// across R_FILT. From (Vgpio - Vnode)/R_FILT = (Vnode - Vref)/R_PWM:
//   Vgpio = Vnode + R_FILT * (Vnode - Vref) / R_PWM
// Note the inversion: higher duty -> higher Vnode -> LOWER Vout.
void FanControl::applyVolts(float v) {
  _volts = constrain(v, BOOST_VOUT_MIN, BOOST_VOUT_MAX);
  float vNode = BOOST_VREF + BOOST_R_PWM * (BOOST_VREF / BOOST_R_BOTTOM -
                                            (_volts - BOOST_VREF) / BOOST_R_TOP);
  float vGpio = vNode + BOOST_R_FILT * (vNode - BOOST_VREF) / BOOST_R_PWM;
  float duty = constrain(vGpio / BOOST_LOGIC_V, 0.0f, 1.0f);
  ledcWrite(LEDC_CHANNEL, (uint32_t)(duty * DUTY_MAX + 0.5f));
}
