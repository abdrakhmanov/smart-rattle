#include <M5Unified.h>

#include <esp_random.h>

#include "Accel.h"
#include "Buzzer.h"
#include "Config.h"
#include "Display.h"
#include "Game.h"
#include "GameManager.h"
#include "Power.h"
#include "games/Fallback.h"
#include "games/Maraca.h"
#include "games/MusicBox.h"
#include "games/TiltPiano.h"

static Accel       accel;
static Buzzer      buzzer;
static Display     display;
static Maraca      maraca;
static MusicBox    musicBox;
static TiltPiano   tiltPiano;
static Fallback    fallback;
static GameManager manager;
static Power       power;
static bool        s_imuFault = false;

static Game* const s_games[] = { &maraca, &musicBox, &tiltPiano };
static constexpr uint8_t kGameCount = sizeof(s_games) / sizeof(s_games[0]);

static uint32_t s_lastTickMs   = 0;
static uint32_t s_lastRenderMs = 0;
static uint32_t s_splashUntil  = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  power.begin();
  randomSeed(esp_random());

  display.begin();
  accel.begin();
  buzzer.begin();

  s_imuFault = !accel.isEnabled();
  display.setFault(s_imuFault);

  const SavedState saved = loadSavedState(power.wokeFromDeepSleep());
  const uint32_t now = millis();
  Context ctx{now, kTickMs, false, accel, buzzer, display};

  Game* const* games = s_games;
  uint8_t count = kGameCount;
  if (s_imuFault) {
    static Game* const faultGames[] = { &fallback };
    games = faultGames;
    count = 1;
  }
  manager.begin(games, count, saved, ctx);
  display.setBrightness(kMaxBrightness);

  power.setSleepPrepare([](void* user) {
    auto* m = static_cast<GameManager*>(user);
    buzzer.stop();
    m->flushPersist();
  }, &manager);

  // Сплэш — состояние с временной меткой, не delay(): цикл продолжает
  // крутиться, и первая тряска обрабатывается даже во время сплэша (§6).
  if (!power.wokeFromDeepSleep()) {
    display.drawBodyMessage(manager.currentName(),
                            s_games[manager.index()]->nameColor());
    s_splashUntil = now + kSplashMs;
  }
}

void loop() {
  const uint32_t now = millis();
  const uint32_t dtMs = (s_lastTickMs == 0) ? kTickMs : (now - s_lastTickMs);
  s_lastTickMs = now;

  M5.update();                                   // 1. кнопки, динамик, питание
  accel.update(now);                             // 2. IMU и фильтры

  // 3. Кнопки: A — смена режима, B — цикл громкости.
  Context btnCtx{now, dtMs, false, accel, buzzer, display};
  if (M5.BtnA.wasClicked() && !power.btnASuppressed()) {
    if (s_imuFault) fallback.strike(btnCtx);
    else            manager.next(btnCtx);
  }
  if (M5.BtnB.wasClicked()) manager.cycleVolume(btnCtx);

  const bool splash = now < s_splashUntil;
  const bool renderTick = !splash && (now - s_lastRenderMs) >= kRenderMs;
  if (renderTick) s_lastRenderMs = now;

  Context ctx{now, dtMs, renderTick, accel, buzzer, display};
  manager.update(ctx);                           // 4. логика режима
  manager.tickPersist(now);
  buzzer.update(now);                            // 5. продвижение нот

  // 6. Простой, яркость, засыпание.
  // §7: простой питается msSinceMotion, а не только событиями тряски —
  // медленное шевеление ребёнком тоже активность.
  const bool activity = (!s_imuFault && (accel.shakeStarted() || accel.isShaking()
                                         || accel.msSinceMotion(now) < kMotionActiveMs))
                     || buzzer.isPlaying()
                     || M5.BtnA.wasPressed() || M5.BtnB.wasPressed();
  power.update(now, activity, display);

  // 7. добрать тик до kTickMs по измеренному времени
  const uint32_t spent = millis() - now;
  M5.delay(spent >= kTickMs ? 1 : (kTickMs - spent));
}
