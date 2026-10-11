#include "Display.h"

#include <M5Unified.h>

namespace {

constexpr uint16_t kBlack = 0x0000;

// Геометрия индикаторов (§9).
constexpr int16_t kVolX = 95, kVolY = 8, kVolW = 12, kVolH = 11;
constexpr int16_t kBatX = 109, kBatY = 8, kBatW = 22, kBatH = 11;

inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return lgfx::color565(r, g, b);
}

}  // namespace

void Display::begin() {
  M5.Display.setRotation(0);                 // портрет 135x240
  M5.Display.setBrightness(kMaxBrightness);
  M5.Display.fillScreen(kBlack);
  // efont: кириллица в UTF-8 без внешних шрифтов и таблиц CP1251 (§9).
  M5.Display.setFont(&lgfx::v1::fonts::efontCN_14);
  M5.Display.setTextSize(1);
  M5.Display.setTextWrap(false);
  _brightness = kMaxBrightness;
  drawBar();
}

uint16_t Display::hsv(float h, float s, float v) {
  while (h < 0.0f) h += 360.0f;
  while (h >= 360.0f) h -= 360.0f;
  if (s < 0.0f) s = 0.0f;
  if (s > 1.0f) s = 1.0f;
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;

  const float c = v * s;
  const float hp = h / 60.0f;
  const int   sector = (int)hp;
  const float f = hp - (float)sector;
  const float x = c * (1.0f - f);
  const float y = c * f;
  const float m = v - c;

  float r = m, g = m, b = m;
  switch (sector % 6) {
    case 0: r = v;     g = m + y; b = m;     break;
    case 1: r = m + x; g = v;     b = m;     break;
    case 2: r = m;     g = v;     b = m + y; break;
    case 3: r = m;     g = m + x; b = v;     break;
    case 4: r = m + y; g = m;     b = v;     break;
    default: r = v;    g = m;     b = m + x; break;
  }
  return rgb((uint8_t)(r * 255.0f), (uint8_t)(g * 255.0f), (uint8_t)(b * 255.0f));
}

void Display::setTitle(const char* name, uint16_t color) {
  if (_title == name && _titleColor == color) return;
  _title = name;
  _titleColor = color;
  drawBar();
}

void Display::setBattery(uint8_t percent, bool charging) {
  if (percent > 100) percent = 100;
  if (_percent == percent && _charging == charging) return;
  _percent = percent;
  _charging = charging;
  drawBar();
}

void Display::setVolume(VolumeLevel v) {
  if (_volume == v) return;
  _volume = v;
  drawBar();
}

void Display::setFault(bool imuFailed) {
  if (_fault == imuFailed) return;
  _fault = imuFailed;
  drawBar();
}

void Display::setBrightness(uint8_t v) {
  if (v > kMaxBrightness) v = kMaxBrightness;   // единственный зажим в проекте
  if (v == _brightness) return;
  _brightness = v;
  M5.Display.setBrightness(v);
}

void Display::fillBody(uint16_t color) {
  M5.Display.fillRect(0, bodyTop(), width(), bodyHeight(), color);
}

void Display::fillBodyRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (y < 0) { h += y; y = 0; }
  if (h <= 0) return;
  if (y + h > bodyHeight()) h = bodyHeight() - y;
  if (h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (w <= 0) return;
  if (x + w > width()) w = width() - x;
  if (w <= 0) return;
  M5.Display.fillRect(x, bodyTop() + y, w, h, c);
}

void Display::clearBody() {
  fillBody(kBlack);
}

void Display::drawBodyMessage(const char* text, uint16_t color) {
  clearBody();
  M5.Display.setTextColor(color, kBlack);
  const int32_t w = M5.Display.textWidth(text);
  const int32_t x = (width() - w) / 2;
  const int32_t y = bodyTop() + bodyHeight() / 2 - 8;
  M5.Display.setCursor(x < 0 ? 0 : x, y);
  M5.Display.print(text);
}

void Display::drawBar() {
  M5.Display.fillRect(0, 0, kScreenWidth, kBarHeight, kBlack);

  // Название режима. При отказе IMU полоса отдаётся маркеру (§12): имени
  // у аварийного режима нет, а причина должна быть видна сразу.
  if (_fault) {
    M5.Display.setTextColor(rgb(0xFF, 0x30, 0x30), kBlack);
    M5.Display.setCursor(4, 6);
    M5.Display.print("IMU");
  } else {
    M5.Display.setTextColor(_titleColor, kBlack);
    M5.Display.setCursor(4, 6);
    M5.Display.print(_title);
  }

  // Индикатор громкости: конус + 0/1/2 волны.
  M5.Display.fillRect(kVolX, kVolY + 3, 3, 5, 0xFFFF);
  M5.Display.fillTriangle(kVolX + 3, kVolY + 5,
                          kVolX + 6, kVolY,
                          kVolX + 6, kVolY + kVolH - 1, 0xFFFF);
  const uint8_t waves = (_volume == VolumeLevel::Normal) ? 2
                      : (_volume == VolumeLevel::Quiet)  ? 1 : 0;
  for (uint8_t i = 0; i < waves; ++i) {
    const int16_t x = kVolX + 8 + i * 2;
    M5.Display.drawFastVLine(x, kVolY + 3 - i, 5 + 2 * i, 0xFFFF);
  }
  if (waves == 0) {
    M5.Display.drawLine(kVolX + 8, kVolY + 2, kVolX + 11, kVolY + 8, rgb(0xFF, 0x30, 0x30));
    M5.Display.drawLine(kVolX + 11, kVolY + 2, kVolX + 8, kVolY + 8, rgb(0xFF, 0x30, 0x30));
  }

  // Индикатор заряда: контур + пропорциональная заливка + цветовая кодировка.
  const uint16_t battColor = (_percent < kBattCritPct) ? rgb(0xFF, 0x30, 0x30)
                           : (_percent < kBattLowPct)  ? rgb(0xFF, 0xC0, 0x20)
                                                       : rgb(0x40, 0xE0, 0x60);
  M5.Display.drawRect(kBatX, kBatY, kBatW - 2, kBatH, 0xFFFF);
  M5.Display.fillRect(kBatX + kBatW - 2, kBatY + 3, 2, 5, 0xFFFF);
  const int16_t inner = kBatW - 6;
  const int16_t fill = (int16_t)((int32_t)inner * _percent / 100);
  if (fill > 0) {
    M5.Display.fillRect(kBatX + 2, kBatY + 2, fill, kBatH - 4, battColor);
  }
  if (_charging) {
    const int16_t cx = kBatX + (kBatW - 2) / 2;
    M5.Display.drawLine(cx + 1, kBatY + 2, cx - 2, kBatY + 6, kBlack);
    M5.Display.drawLine(cx - 2, kBatY + 6, cx + 2, kBatY + 5, kBlack);
    M5.Display.drawLine(cx + 2, kBatY + 5, cx - 1, kBatY + 9, kBlack);
  }
}

void Display::sleep() {
  M5.Display.sleep();
}
