#pragma once
#include <stdint.h>

#include "Config.h"
#include "Display.h"

enum class PowerState : uint8_t { Awake, Dimmed };

// Единственный владелец политики сна. Режимы его не видят, иначе любой
// режим мог бы незаметно запретить засыпание (§4).
class Power {
 public:
  using SleepPrepare = void (*)(void* user);

  void begin();

  bool wokeFromDeepSleep() const { return _wokeFromSleep; }

  // Вызывается перед засыпанием: остановить звук и сбросить NVS.
  void setSleepPrepare(SleepPrepare cb, void* user) { _prepare = cb; _user = user; }

  void update(uint32_t now, bool activity, Display& display);

  // Пробуждающее нажатие A не должно прилететь в GameManager как «переключи
  // режим»: флаг снимается только после того, как A увидена отпущенной (§6).
  bool btnASuppressed() const { return _suppressBtnA; }

  uint8_t    batteryPercent() const { return _percent; }
  bool       charging()       const { return _charging; }
  PowerState state()          const { return _state; }

 private:
  void pollBattery(uint32_t now, Display& display);
  void applyBrightness(Display& display);
  void enterDeepSleep(Display& display);

  SleepPrepare _prepare = nullptr;
  void*        _user    = nullptr;

  PowerState _state         = PowerState::Awake;
  bool       _wokeFromSleep = false;
  bool       _suppressBtnA  = false;
  bool       _charging      = false;
  uint8_t    _percent       = 100;
  uint8_t    _lowStreak     = 0;
  uint32_t   _lastActiveMs  = 0;
  uint32_t   _lastBattMs    = 0;
  uint32_t   _warnUntilMs   = 0;
  bool       _warning       = false;
};
