#pragma once
#include <Arduino.h>
#include "Controller.h"

// 72x40 SSD1306 UI. Main screen: temp, mode+level, duty bar, RPM.
// A change popup takes over the full screen for POPUP_DURATION_MS.
class DisplayUi {
public:
  void begin();
  void showChangePopup(Mode mode, uint8_t level, uint32_t now);
  void tick(uint32_t now, float tempC, bool tempValid, Mode mode,
            uint8_t level, float dutyPct, uint32_t rpm, bool stalled);

private:
  void drawMain(float tempC, bool tempValid, Mode mode, uint8_t level,
                float dutyPct, uint32_t rpm, bool stalled);
  void drawPopup();

  uint32_t _popupUntil = 0;
  Mode _popupMode = Mode::Auto;
  uint8_t _popupLevel = 0;
  uint32_t _lastDraw = 0;
};
