#pragma once
#include <stdint.h>

#include "Config.h"

// Единственный владелец M5.Display. Полоса сверху + тело экрана.
class Display {
 public:
  void begin();

  void setTitle(const char* name, uint16_t color);
  void setBattery(uint8_t percent, bool charging);
  void setVolume(VolumeLevel v);
  void setFault(bool imuFailed);           // §12: красный маркер IMU

  void fillBody(uint16_t color);
  void fillBodyRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
  void clearBody();
  void drawBodyMessage(const char* text, uint16_t color);

  void setBrightness(uint8_t v);           // зажимается kMaxBrightness
  void sleep();                            // гасит панель перед deep sleep

  int16_t bodyTop()    const { return kBarHeight; }
  int16_t bodyHeight() const { return kScreenHeight - kBarHeight; }
  int16_t width()      const { return kScreenWidth; }

  static uint16_t hsv(float h, float s, float v);

 private:
  void drawBar();

  const char* _title      = "";
  uint16_t    _titleColor = 0xFFFF;
  uint8_t     _percent    = 100;
  bool        _charging   = false;
  VolumeLevel _volume     = VolumeLevel::Quiet;
  bool        _fault      = false;
  uint8_t     _brightness = kMaxBrightness;
};
