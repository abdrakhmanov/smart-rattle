#include <M5Unified.h>

#include <esp_random.h>

#include "Accel.h"
#include "Buzzer.h"
#include "Config.h"
#include "Display.h"
#include "Game.h"
#include "GameManager.h"
#include "games/Maraca.h"

static Accel       accel;
static Buzzer      buzzer;
static Display     display;
static Maraca      maraca;
static GameManager manager;

static Game* const s_games[] = { &maraca };   // MusicBox и TiltPiano — Tasks 12, 13
static constexpr uint8_t kGameCount = sizeof(s_games) / sizeof(s_games[0]);

static uint32_t s_lastTickMs   = 0;
static uint32_t s_lastRenderMs = 0;
static uint32_t s_lastBattMs   = 0;
static uint32_t s_splashUntil  = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  randomSeed(esp_random());

  display.begin();
  accel.begin();
  buzzer.begin();

  const SavedState saved = loadSavedState(false);   // wake cause — Task 11
  const uint32_t now = millis();
  Context ctx{now, kTickMs, false, accel, buzzer, display};
  manager.begin(s_games, kGameCount, saved, ctx);
  display.setBrightness(kMaxBrightness);

  // Сплэш — состояние с временной меткой, не delay(): цикл продолжает
  // крутиться, и первая тряска обрабатывается даже во время сплэша (§6).
  display.drawBodyMessage(manager.currentName(), s_games[manager.index()]->nameColor());
  s_splashUntil = now + kSplashMs;
}

void loop() {
  const uint32_t now = millis();
  const uint32_t dtMs = (s_lastTickMs == 0) ? kTickMs : (now - s_lastTickMs);
  s_lastTickMs = now;

  M5.update();                                   // 1. кнопки, динамик, питание
  accel.update(now);                             // 2. IMU и фильтры

  // 3. Кнопки: A — смена режима, B — цикл громкости.
  Context btnCtx{now, dtMs, false, accel, buzzer, display};
  if (M5.BtnA.wasClicked()) manager.next(btnCtx);
  if (M5.BtnB.wasClicked()) manager.cycleVolume(btnCtx);

  const bool splash = now < s_splashUntil;
  const bool renderTick = !splash && (now - s_lastRenderMs) >= kRenderMs;
  if (renderTick) s_lastRenderMs = now;

  Context ctx{now, dtMs, renderTick, accel, buzzer, display};
  manager.update(ctx);                           // 4. логика режима
  manager.tickPersist(now);
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
