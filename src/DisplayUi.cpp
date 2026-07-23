#include "DisplayUi.h"
#include "config.h"
#include <U8g2lib.h>

static U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE,
                                           PIN_OLED_SCL, PIN_OLED_SDA);

void DisplayUi::begin() {
  u8g2.begin();
  u8g2.setBusClock(400000);  //400kHz I2C
  // Nudge the panel alignment for this specific (clone) glass — see the
  // comment on OLED_X_OFFSET/OLED_Y_OFFSET in config.h.
  u8g2.getU8x8()->x_offset = OLED_X_OFFSET;
  u8g2.sendF("ca", 0x0d3, OLED_Y_OFFSET); // SSD1306 "Set Display Offset"
  u8g2.setContrast(255);     // set contrast to maximum
}

void DisplayUi::showChangePopup(Mode mode, uint8_t level, uint32_t now) {
  _popupMode = mode;
  _popupLevel = level;
  _popupUntil = now + POPUP_DURATION_MS;
  _lastDraw = 0; // redraw immediately
}

void DisplayUi::tick(uint32_t now, float tempC, bool tempValid, Mode mode,
                     uint8_t level, float powerPct, float volts, uint32_t rpm,
                     bool stalled) {
  if (now - _lastDraw < 100) return; // ~10 Hz
  _lastDraw = now;
  u8g2.clearBuffer();
  if (now < _popupUntil)
    drawPopup();
  else
    drawMain(tempC, tempValid, mode, level, powerPct, volts, rpm, stalled);
  u8g2.sendBuffer();
}

// +--------------------------+
// | +----------------------+ |  full-screen takeover, framed so a change
// | |        A 3           | |  is unmistakable at a glance:
// | |   logisoso22 (big)   | |    line 1: mode letter + level, centered
// | |        AUTO          | |    line 2: mode spelled out, 5x8 font
// | +----------------------+ |
// +--------------------------+
void DisplayUi::drawPopup() {
  char big[8];
  snprintf(big, sizeof(big), "%s %u",
           _popupMode == Mode::Auto ? "A" : "M", _popupLevel);
  u8g2.setFont(u8g2_font_logisoso22_tr);
  u8g2.drawStr((72 - u8g2.getStrWidth(big)) / 2, 31, big);
  u8g2.setFont(u8g2_font_5x8_tr);
  const char* label = _popupMode == Mode::Auto ? "AUTO" : "MANUAL";
  u8g2.drawStr((72 - u8g2.getStrWidth(label)) / 2, 39, label);
  u8g2.drawFrame(0, 0, 72, 40);
}

// +--------------------------+
// |  24.3°              A3   |  y=16: temp logisoso16 left, mode+level right
// |  [#########_______]      |  y=21..29: power bar = live fan power
// |  1450rpm           9.5V  |  y=39: rpm left, fan DC volts right (6x10);
// +--------------------------+        replaced by "FAN STALL!" on stall
void DisplayUi::drawMain(float tempC, bool tempValid, Mode mode, uint8_t level,
                         float powerPct, float volts, uint32_t rpm,
                         bool stalled) {
  // Temperature, large
  char buf[16];
  if (tempValid)
    snprintf(buf, sizeof(buf), "%.1f\xb0", tempC);
  else
    snprintf(buf, sizeof(buf), "--.-");
  u8g2.setFont(u8g2_font_logisoso16_tf);
  u8g2.drawUTF8(0, 16, buf);

  // Mode + level, top right
  snprintf(buf, sizeof(buf), "%s%u", mode == Mode::Auto ? "A" : "M", level);
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(72 - u8g2.getStrWidth(buf), 12, buf);

  // Power bar with current output
  u8g2.drawFrame(0, 21, 72, 8);
  u8g2.drawBox(1, 22, (uint8_t)(70.0f * powerPct / 100.0f), 6);

  // Bottom line: stall warning or RPM + fan supply voltage
  u8g2.setFont(u8g2_font_6x10_tr);
  if (stalled) {
    const char* warn = "FAN STALL!";
    u8g2.drawStr((72 - u8g2.getStrWidth(warn)) / 2, 39, warn);
  } else {
    snprintf(buf, sizeof(buf), "%urpm", (unsigned)rpm);
    u8g2.drawStr(0, 39, buf);
    snprintf(buf, sizeof(buf), "%.1fV", volts);
    u8g2.drawStr(72 - u8g2.getStrWidth(buf), 39, buf);
  }
}
