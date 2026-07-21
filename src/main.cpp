#include <Arduino.h>
#include "config.h"
#include "TempSensor.h"
#include "FanControl.h"
#include "Tach.h"
#include "ButtonInput.h"
#include "Controller.h"
#include "DisplayUi.h"

static TempSensor tempSensor;
static FanControl fan;
static Tach tach;
static ButtonInput button;
static Controller controller;
static DisplayUi ui;

static uint32_t spinningSince = 0; // last time we saw rpm > 0 (or power == 0)
static bool stalled = false;
static uint32_t lastLog = 0;

void setup() {
  // Fan first: until the FB-injection PWM runs, the buck free-runs at ~14 V.
  fan.begin();
  Serial.begin(115200);
  tempSensor.begin();
  tach.begin();
  button.begin();
  ui.begin();
  ui.showChangePopup(controller.mode(), controller.level(), millis());
}

void loop() {
  uint32_t now = millis();

  tempSensor.tick(now);
  tach.tick(now);

  if (controller.handleButton(button.tick(now)))
    ui.showChangePopup(controller.mode(), controller.level(), now);

  float power = controller.computePowerPercent(tempSensor.celsius(),
                                               tempSensor.valid());
  fan.setPowerPercent(power);

  // Stall detection: commanded on but no tach pulses for STALL_TIMEOUT_MS.
  if (power <= 0.0f || tach.rpm() > 0) spinningSince = now;
  stalled = (now - spinningSince) > STALL_TIMEOUT_MS;

  ui.tick(now, tempSensor.celsius(), tempSensor.valid(), controller.mode(),
          controller.level(), power, fan.targetVolts(), tach.rpm(), stalled);

  if (now - lastLog >= 2000) {
    lastLog = now;
    Serial.printf("temp=%.1fC mode=%s level=%u power=%.0f%% volts=%.1fV rpm=%u%s\n",
                  tempSensor.celsius(),
                  controller.mode() == Mode::Auto ? "auto" : "manual",
                  controller.level(), power, fan.targetVolts(), tach.rpm(),
                  stalled ? " STALL" : "");
  }
}
