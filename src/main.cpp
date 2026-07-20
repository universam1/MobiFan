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

static uint32_t spinningSince = 0; // last time we saw rpm > 0 (or duty == 0)
static bool stalled = false;
static uint32_t lastLog = 0;

void setup() {
  Serial.begin(115200);
  tempSensor.begin();
  fan.begin();
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

  float duty = controller.computeDutyPercent(tempSensor.celsius(),
                                             tempSensor.valid());
  fan.setDutyPercent(duty);

  // Stall detection: commanded on but no tach pulses for STALL_TIMEOUT_MS.
  if (duty <= 0.0f || tach.rpm() > 0) spinningSince = now;
  stalled = (now - spinningSince) > STALL_TIMEOUT_MS;

  ui.tick(now, tempSensor.celsius(), tempSensor.valid(), controller.mode(),
          controller.level(), duty, tach.rpm(), stalled);

  if (now - lastLog >= 2000) {
    lastLog = now;
    Serial.printf("temp=%.1fC mode=%s level=%u duty=%.0f%% rpm=%u%s\n",
                  tempSensor.celsius(),
                  controller.mode() == Mode::Auto ? "auto" : "manual",
                  controller.level(), duty, tach.rpm(),
                  stalled ? " STALL" : "");
  }
}
