#pragma once
#include <stdint.h>

#include "Accel.h"
#include "Buzzer.h"
#include "Display.h"

// Режимы получают accel/buzzer/display и НЕ получают Power: политика сна
// целиком принадлежит Power, иначе любой режим мог бы незаметно запретить сон.
struct Context {
  uint32_t now;
  uint32_t dtMs;
  bool     renderTick;     // истинно ~20 раз в секунду
  Accel&   accel;
  Buzzer&  buzzer;
  Display& display;
};

class Game {
 public:
  virtual ~Game() = default;
  virtual const char* name()      const = 0;
  virtual uint16_t    nameColor() const = 0;
  virtual void onEnter(Context&) = 0;
  virtual void onExit (Context&) = 0;
  virtual void update (Context&) = 0;
};
