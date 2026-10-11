#pragma once
#include <stdint.h>

#include "Game.h"

// «Пианино»: наклон задаёт ступень, звук идёт только пока устройство
// двигается. Полоса остаётся на экране и в тишине.
class TiltPiano : public Game {
 public:
  const char* name()      const override { return "Пианино"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

 private:
  void drawBar(Context& ctx, int16_t height, uint16_t color, bool full);

  int8_t   _step       = -1;
  int16_t  _prevHeight = 0;
  uint32_t _silentAtMs = 0;
  bool     _sounding   = false;
};
