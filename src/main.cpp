// Визуальный пробник Display. Заменяется штатным циклом в Task 9.
#include <M5Unified.h>

#include "Config.h"
#include "Display.h"

static Display display;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  display.begin();
  display.setTitle("Погремушка", Display::hsv(35.0f, 0.75f, 1.0f));
  display.setVolume(VolumeLevel::Quiet);
  display.setBattery(100, false);
  display.clearBody();
}

void loop() {
  static uint32_t phase = 0;
  static float hue = 0.0f;
  M5.update();
  const uint32_t now = millis();

  // Каждые 2 с меняем то, что показывает полоса.
  if (now / 2000 != phase) {
    phase = now / 2000;
    switch (phase % 6) {
      case 0: display.setTitle("Погремушка", Display::hsv(35.0f, 0.75f, 1.0f));
              display.setVolume(VolumeLevel::Quiet);
              display.setBattery(100, false); break;
      case 1: display.setTitle("Шкатулка", Display::hsv(250.0f, 0.4f, 1.0f));
              display.setVolume(VolumeLevel::Normal);
              display.setBattery(45, false); break;
      case 2: display.setTitle("Пианино", Display::hsv(145.0f, 0.6f, 0.9f));
              display.setVolume(VolumeLevel::Mute);
              display.setBattery(18, false); break;
      case 3: display.setBattery(8, false); break;
      case 4: display.setBattery(60, true); break;
      default:
        display.drawBodyMessage("Заряд", Display::hsv(0.0f, 1.0f, 1.0f));
        break;
    }
  }

  // Плавный свип тела: 8 с на полный круг — медленнее 0,5 Гц.
  if ((phase % 6) != 5) {
    hue += 360.0f * (float)kRenderMs / (float)kSweepPeriodFastMs;
    display.fillBody(Display::hsv(hue, 0.5f, 1.0f));
  }
  M5.delay(kRenderMs);
}
