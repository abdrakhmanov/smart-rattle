#include <M5Unified.h>

#include <esp_random.h>

#include "Accel.h"
#include "Buzzer.h"
#include "Config.h"
#include "Display.h"
#include "Game.h"
#include "games/Maraca.h"

static Accel   accel;
static Buzzer  buzzer;
static Display display;
static Maraca  maraca;

static uint32_t s_lastTickMs   = 0;
static uint32_t s_lastRenderMs = 0;
static uint32_t s_lastBattMs   = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  randomSeed(esp_random());

  display.begin();
  accel.begin();
  buzzer.begin();
  buzzer.setVolumeLevel(VolumeLevel::Quiet);

  display.setTitle(maraca.name(), maraca.nameColor());
  display.setVolume(VolumeLevel::Quiet);
  display.setBrightness(kMaxBrightness);

  const uint32_t now = millis();
  Context ctx{now, kTickMs, false, accel, buzzer, display};
  maraca.onEnter(ctx);
}

void loop() {
  const uint32_t now = millis();
  const uint32_t dtMs = (s_lastTickMs == 0) ? kTickMs : (now - s_lastTickMs);
  s_lastTickMs = now;

  M5.update();                                   // 1. кнопки, динамик, питание
  accel.update(now);                             // 2. IMU и фильтры
                                                 // 3. кнопки — Task 10

  const bool renderTick = (now - s_lastRenderMs) >= kRenderMs;
  if (renderTick) s_lastRenderMs = now;

  Context ctx{now, dtMs, renderTick, accel, buzzer, display};
  maraca.update(ctx);                            // 4. логика режима
  buzzer.update(now);                            // 5. продвижение нот
                                                 // 6. power.update — Task 11

  // Временно здесь: в Task 11 опрос батареи переезжает в Power.
  if (now - s_lastBattMs >= kBatteryPollMs) {
    s_lastBattMs = now;
    int32_t level = M5.Power.getBatteryLevel();
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    display.setBattery((uint8_t)level,
                       M5.Power.isCharging() == m5::Power_Class::is_charging);
  }

  // 7. добрать тик до kTickMs по измеренному времени
  const uint32_t spent = millis() - now;
  M5.delay(spent >= kTickMs ? 1 : (kTickMs - spent));
}
