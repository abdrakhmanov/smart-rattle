#include "GameManager.h"

#include <M5Unified.h>
#include <Preferences.h>

namespace {

constexpr uint32_t kRtcMagic = 0x4D524131u;   // 'MRA1'
constexpr const char* kNvsNamespace = "maraca";
constexpr const char* kNvsKeyGame   = "game";
constexpr const char* kNvsKeyVolume = "vol";

RTC_DATA_ATTR uint32_t s_rtcMagic  = 0;
RTC_DATA_ATTR uint8_t  s_rtcIndex  = 0;
RTC_DATA_ATTR uint8_t  s_rtcVolume = 0;

inline VolumeLevel volumeFromRaw(uint8_t raw) {
  return (raw > (uint8_t)VolumeLevel::Mute) ? VolumeLevel::Quiet : (VolumeLevel)raw;
}

}  // namespace

SavedState loadSavedState(bool wokeFromDeepSleep) {
  if (wokeFromDeepSleep && s_rtcMagic == kRtcMagic) {
    return SavedState{s_rtcIndex, volumeFromRaw(s_rtcVolume)};
  }

  SavedState st{0, VolumeLevel::Quiet};        // §12: пустая или битая NVS
  Preferences prefs;
  if (prefs.begin(kNvsNamespace, true)) {
    st.gameIndex = prefs.getUChar(kNvsKeyGame, 0);
    st.volume    = volumeFromRaw(prefs.getUChar(kNvsKeyVolume,
                                                (uint8_t)VolumeLevel::Quiet));
    prefs.end();
  }
  return st;
}

void GameManager::begin(Game* const* games, uint8_t count,
                        const SavedState& saved, Context& ctx) {
  _games = games;
  _count = count;
  _index = (count == 0 || saved.gameIndex >= count) ? 0 : saved.gameIndex;
  _volume = saved.volume;
  _dirty = false;

  ctx.buzzer.setVolumeLevel(_volume);
  ctx.display.setVolume(_volume);
  ctx.display.setTitle(currentName(), _games[_index]->nameColor());
  ctx.display.clearBody();
  _games[_index]->onEnter(ctx);
}

void GameManager::update(Context& ctx) {
  _games[_index]->update(ctx);
}

void GameManager::next(Context& ctx) {
  ctx.buzzer.stop();
  _games[_index]->onExit(ctx);
  _index = (uint8_t)((_index + 1) % _count);

  // Полоса и тело готовятся до onEnter, чтобы отрисовка режима не стиралась.
  ctx.display.setTitle(currentName(), _games[_index]->nameColor());
  ctx.display.clearBody();
  _games[_index]->onEnter(ctx);

  // Двухнотный восходящий подтверждающий сигнал: это намеренное действие
  // взрослого, и оно требует подтверждения (§10).
  const Note confirm[2] = {
    {kConfirmStepLow,  kConfirmNoteMs, 0},
    {kConfirmStepHigh, kConfirmNoteMs, 0},
  };
  ctx.buzzer.play(confirm, 2);

  markDirty(ctx.now);
}

void GameManager::cycleVolume(Context& ctx) {
  switch (_volume) {
    case VolumeLevel::Quiet:  _volume = VolumeLevel::Normal; break;
    case VolumeLevel::Normal: _volume = VolumeLevel::Mute;   break;
    default:                  _volume = VolumeLevel::Quiet;  break;
  }
  ctx.buzzer.setVolumeLevel(_volume);
  ctx.display.setVolume(_volume);
  markDirty(ctx.now);
}

void GameManager::markDirty(uint32_t now) {
  // RTC-память — это RAM, писать в неё можно на каждое изменение.
  s_rtcMagic  = kRtcMagic;
  s_rtcIndex  = _index;
  s_rtcVolume = (uint8_t)_volume;

  _dirty = true;
  _dirtyAtMs = now;
}

void GameManager::tickPersist(uint32_t now) {
  if (!_dirty) return;
  if (now - _dirtyAtMs < kNvsDebounceMs) return;
  flushPersist();
}

void GameManager::flushPersist() {
  if (!_dirty) return;
  // Дебаунс обязателен: запись на каждое нажатие дала бы сотни циклов
  // стирания флеша за вечер (§10).
  Preferences prefs;
  if (prefs.begin(kNvsNamespace, false)) {
    prefs.putUChar(kNvsKeyGame, _index);
    prefs.putUChar(kNvsKeyVolume, (uint8_t)_volume);
    prefs.end();
  }
  _dirty = false;
}
