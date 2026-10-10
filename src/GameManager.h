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
  void begin(Game* const* games, uint8_t count, const SavedState& saved, Context& ctx);
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
};
