#pragma once
#include <stddef.h>
#include <stdint.h>

#include "Config.h"

struct Note {
  int16_t  step;    // ступень пентатоники; отрицательная = пауза
  uint16_t durMs;
  uint16_t gapMs;
};

// Секвенсор нот поверх M5.Speaker: темп с паузами, пентатоническое
// квантование, потолок громкости. Единственный владелец M5.Speaker.
class Buzzer {
 public:
  virtual ~Buzzer() = default;

  void   begin();
  void   update(uint32_t now);
  void   play(const Note* notes, size_t n);   // очистить очередь и проиграть
  void   push(const Note& n);                 // досыпать в хвост
  void   stop();
  bool   isPlaying() const;
  void   setTempoScale(float s);              // растягивает durMs и gapMs
  size_t pending() const { return _count; }

  static float pitchOf(int step);

 protected:
  // Швы для нативных тестов. На железе бьют в M5.Speaker.
  virtual void emitTone(float hz, uint16_t durMs);
  virtual void emitStop();

  Note     _ring[kNoteCapacity];
  size_t   _head   = 0;
  size_t   _count  = 0;
  uint32_t _nextMs = 0;        // когда можно выдать следующую ноту
  uint32_t _lastNow = 0;
  float    _tempo  = 1.0f;
};
