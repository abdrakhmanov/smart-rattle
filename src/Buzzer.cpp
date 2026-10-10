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
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;        // первая нота звучит сразу
  for (size_t i = 0; i < n; ++i) push(notes[i]);
}

void Buzzer::push(const Note& n) {
  if (_count >= kNoteCapacity) return;   // §12: новая нота отбрасывается
  _ring[(_head + _count) % kNoteCapacity] = n;
  ++_count;
}

void Buzzer::stop() {
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;
  emitStop();
}

bool Buzzer::isPlaying() const {
  return _count > 0 || _lastNow < _nextMs;
}

void Buzzer::setTempoScale(float s) {
  if (s < 0.25f) s = 0.25f;
  if (s > 4.0f)  s = 4.0f;
  _tempo = s;
}

void Buzzer::update(uint32_t now) {
  _lastNow = now;
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
