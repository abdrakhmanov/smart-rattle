#include "Buzzer.h"

#include <math.h>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

namespace {
inline uint16_t scaled(uint16_t ms, float tempo) {
  const float v = (float)ms * tempo;
  return (uint16_t)(v < 0.0f ? 0.0f : (v > 65535.0f ? 65535.0f : v));
}
}  // namespace

void Buzzer::begin() {
  _head = 0;
  _count = 0;
  _nextMs = 0;
  _lastNow = 0;
  _tempo = 1.0f;
  _gliding = false;
  _glideHz = 0.0f;
  setVolumeLevel(VolumeLevel::Quiet);   // §12: громкость по умолчанию
}

float Buzzer::pitchOf(int step) {
  if (step < 0) step = 0;
  if (step > kStepMax) step = kStepMax;
  const int octave = step / 5;
  const int index  = step % 5;
  const float semitone = (float)kScale[index] + 12.0f * (float)octave;
  return kBaseHz * powf(2.0f, semitone / 12.0f);
}

void Buzzer::play(const Note* notes, size_t n) {
  if (_gliding) { _gliding = false; emitStop(); }
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;        // первая нота звучит сразу
  for (size_t i = 0; i < n; ++i) push(notes[i]);
}

void Buzzer::push(const Note& n) {
  if (_gliding) { _gliding = false; emitStop(); }
  if (_count >= kNoteCapacity) return;   // §12: новая нота отбрасывается
  _ring[(_head + _count) % kNoteCapacity] = n;
  ++_count;
}

void Buzzer::stop() {
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;
  _gliding = false;
  _glideHz = 0.0f;
  emitStop();
}

bool Buzzer::isPlaying() const {
  return _gliding || _count > 0 || _lastNow < _nextMs;
}

void Buzzer::setTempoScale(float s) {
  if (s < 0.25f) s = 0.25f;
  if (s > 4.0f)  s = 4.0f;
  _tempo = s;
}

void Buzzer::update(uint32_t now) {
  _lastNow = now;
  if (_gliding) return;   // динамик в исключительном владении глиссандо
  if (_count == 0) return;
  if (now < _nextMs) return;

  const Note n = _ring[_head];
  _head = (_head + 1) % kNoteCapacity;
  --_count;

  const uint16_t dur = scaled(n.durMs, _tempo);
  const uint16_t gap = scaled(n.gapMs, _tempo);
  if (n.step >= 0) emitTone(pitchOf(n.step), dur);
  _nextMs = now + dur + gap;
}

void Buzzer::emitTone(float hz, uint16_t durMs) {
#if defined(ARDUINO)
  // stop_current_sound = false: обрыв предыдущей ноты на полуфазе даёт щелчок.
  M5.Speaker.tone(hz, durMs, -1, false);
#else
  (void)hz; (void)durMs;
#endif
}

void Buzzer::emitStop() {
#if defined(ARDUINO)
  M5.Speaker.stop();
#endif
}

void Buzzer::glide(float hz) {
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;
  // Повторная выдача тона на той же частоте дала бы щелчок на ровном месте.
  if (!_gliding || fabsf(hz - _glideHz) > 1.0f) {
    _glideHz = hz;
    emitGlide(hz);
  }
  _gliding = true;
}

void Buzzer::setVolumeLevel(VolumeLevel v) {
  switch (v) {
    case VolumeLevel::Quiet:
      _volume = (uint8_t)((uint16_t)kMaxMasterVolume * kVolumeQuietPct / 100);
      break;
    case VolumeLevel::Normal:
      _volume = kMaxMasterVolume;
      break;
    case VolumeLevel::Mute:
    default:
      _volume = 0;
      break;
  }
  applyVolume(_volume);
}

void Buzzer::emitGlide(float hz) {
#if defined(ARDUINO)
  // Бесконечный тон на выделенном канале; смена частоты перебивает прежний.
  M5.Speaker.tone(hz, UINT32_MAX, 0, true);
#else
  (void)hz;
#endif
}

void Buzzer::applyVolume(uint8_t v) {
#if defined(ARDUINO)
  // Единственное место в проекте, где вызывается setVolume (§8).
  M5.Speaker.setVolume(v);
#else
  (void)v;
#endif
}
