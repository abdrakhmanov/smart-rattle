#pragma once
#include <stdint.h>

#include "Config.h"
#include "Game.h"

struct SavedState {
  uint8_t     gameIndex;
  VolumeLevel volume;
};

// Пробуждение из сна берёт состояние из RTC_DATA_ATTR (при совпадении
// магического числа), холодный старт — из NVS. Несовпадение магии
// трактуется как холодный старт (§12).
SavedState loadSavedState(bool wokeFromDeepSleep);

class GameManager {
 public:
  // restrictedList=true — список игр ограничен (например, аварийный режим
  // без IMU): _index в этом списке не соответствует реальному сохранённому
  // игровому режиму и не должен персиститься (final review, Minor).
  void begin(Game* const* games, uint8_t count, const SavedState& saved, Context& ctx,
             bool restrictedList = false);
  void update(Context& ctx);

  void next(Context& ctx);
  void cycleVolume(Context& ctx);

  void tickPersist(uint32_t now);
  void flushPersist();

  uint8_t     index()  const { return _index; }
  VolumeLevel volume() const { return _volume; }
  const char* currentName() const { return _games[_index]->name(); }

 private:
  void markDirty(uint32_t now);

  Game* const* _games = nullptr;
  uint8_t      _count = 0;
  uint8_t      _index = 0;
  VolumeLevel  _volume = VolumeLevel::Quiet;
  bool         _dirty = false;
  uint32_t     _dirtyAtMs = 0;
  // Истинно, когда begin() получил ограниченный (аварийный) список игр —
  // в этом режиме _index бессмыслен за пределами fault-списка и не должен
  // попадать в RTC/NVS, иначе затирает сохранённый пользователем реальный
  // режим нулём (final review, Minor).
  bool         _restrictedList = false;
};
