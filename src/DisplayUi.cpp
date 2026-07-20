#include "DisplayUi.h"
#include "config.h"
#include <U8g2lib.h>

static U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE,
                                           PIN_OLED_SCL, PIN_OLED_SDA);

void DisplayUi::begin() {
  u8g2.begin();
  u8g2.setContrast(255);
}

void DisplayUi::showChangePopup(Mode mode, uint8_t level, uint32_t now) {
  _popupMode = mode;
  _popupLevel = level;
  _popupUntil = now + POPUP_DURATION_MS;
  _lastDraw = 0; // redraw immediately
}

void DisplayUi::tick(uint32_t now, float tempC, bool tempValid, Mode mode,
                     uint8_t level, float dutyPct, uint32_t rpm, bool stalled) {
  if (now - _lastDraw < 100) return; // ~10 Hz
  _lastDraw = now;
  u8g2.clearBuffer();
  if (now < _popupUntil)
    drawPopup();
  else
    drawMain(tempC, tempValid, mode, level, dutyPct, rpm, stalled);
  u8g2.sendBuffer();
}

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

void DisplayUi::drawMain(float tempC, bool tempValid, Mode mode, uint8_t level,
                         float dutyPct, uint32_t rpm, bool stalled) {
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

  // Duty bar with current output
  u8g2.drawFrame(0, 21, 72, 8);
  u8g2.drawBox(1, 22, (uint8_t)(70.0f * dutyPct / 100.0f), 6);

  // Bottom line: stall warning or RPM + duty
  u8g2.setFont(u8g2_font_5x8_tr);
  if (stalled) {
    u8g2.drawStr(0, 39, "! FAN STALL !");
  } else {
    snprintf(buf, sizeof(buf), "%urpm", (unsigned)rpm);
    u8g2.drawStr(0, 39, buf);
    snprintf(buf, sizeof(buf), "%d%%", (int)(dutyPct + 0.5f));
    u8g2.drawStr(72 - u8g2.getStrWidth(buf), 39, buf);
  }
}
