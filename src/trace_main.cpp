// Прошивка для снятия трасс и калибровки kTiltAxis.
// Собирается только в окружении env:trace (см. platformio.ini).
#include <M5Unified.h>

#include "Accel.h"
#include "Config.h"

static Accel accel;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(0);
  M5.Display.setBrightness(kMaxBrightness);
  M5.Display.fillScreen(0x0000);
  M5.Display.setTextColor(0xFFFF, 0x0000);
  accel.begin();
  if (!accel.isEnabled()) {
    M5.Display.setCursor(4, 4);
    M5.Display.print("NO IMU");
  }
}

void loop() {
  const uint32_t now = millis();
  M5.update();
  accel.update(now);

  // Калибровочная подсказка на экране: раз в 250 мс, вне CSV.
  static uint32_t lastShow = 0;
  if (now - lastShow >= 250) {
    lastShow = now;
    M5.Display.fillRect(0, 0, kScreenWidth, 60, 0x0000);
    M5.Display.setCursor(2, 2);
    M5.Display.printf("tilt %5.1f\nrate %5.0f\nlvl  %4.2f",
                      accel.tiltAngle(), accel.motionRate(), accel.shakeLevel());
  }
  M5.delay(kTickMs);
}
