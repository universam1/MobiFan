#include "FanControl.h"
#include "config.h"
#include "driver/gpio.h"

static constexpr uint8_t LEDC_CHANNEL = 0;
static constexpr uint32_t DUTY_MAX = (1u << FAN_PWM_RES_BITS) - 1;

void FanControl::begin() {
  ledcSetup(LEDC_CHANNEL, FAN_PWM_FREQ_HZ, FAN_PWM_RES_BITS);
  ledcAttachPin(PIN_FAN_PWM, LEDC_CHANNEL);
  // Open-drain after attach: keeps the LEDC matrix routing, releases the
  // line to the fan's 5V pull-up on "high".
  gpio_set_direction((gpio_num_t)PIN_FAN_PWM, GPIO_MODE_OUTPUT_OD);
  setDutyPercent(0);
}

void FanControl::setDutyPercent(float pct) {
  _duty = constrain(pct, 0.0f, 100.0f);
  ledcWrite(LEDC_CHANNEL, (uint32_t)(_duty / 100.0f * DUTY_MAX + 0.5f));
}
