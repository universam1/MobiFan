#pragma once
#include <Arduino.h>
#include "Controller.h"

// 72x40 SSD1306 UI. Main screen: temp, mode+level, duty bar, RPM.
// A change popup takes over the full screen for POPUP_DURATION_MS.
//
// Main screen (normal):            Main screen (fan stalled):
//   +--------------72px--------+     +---------------------------+
//   |                          |     |                           |
//   |  24.3°              A3   |     |  24.3°              M2    |
//   |                          |     |                           |
//   |  [#########_______]      |     |  [#####______________]    |
//   |                          |     |                           |
//   |  1450rpm            64%  |     |  ! FAN STALL !            |
//   +--------------------------+     +---------------------------+
//    temp (large)   mode+level        bottom line replaced by
//    duty bar = live PWM output       warning while stalled
//    rpm from tach    duty %
//
// Change popup (shown 1.5 s after any level/mode change, framed):
//   +--------------------------+     +---------------------------+
//   | +----------------------+ |     | +-----------------------+ |
//   | |                      | |     | |                       | |
//   | |        A 3           | |     | |        M 4            | |
//   | |      (large)         | |     | |      (large)          | |
//   | |        AUTO          | |     | |       MANUAL          | |
//   | +----------------------+ |     | +-----------------------+ |
//   +--------------------------+     +---------------------------+
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
