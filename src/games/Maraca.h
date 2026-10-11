#pragma once
#include <stdint.h>

#include "Game.h"

// «Погремушка»: нота и цвет на фронт тряски, досыпание нот пока трясут,
// затухание в чёрный по окончании.
class Maraca : public Game {
 public:
  const char* name()      const override { return "Погремушка"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

 private:
  void startBurst(Context& ctx);

  int8_t   _paletteIndex = -1;    // чтобы два соседних выбора не повторялись
  float    _hue          = 0.0f;
  float    _levelEnv     = 0.0f;  // огибающая яркости, tau = kMaracaLevelTauMs
  uint32_t _nextNoteMs   = 0;
  uint32_t _fadeStartMs  = 0;
  uint32_t _lastHueMs    = 0;     // когда последний раз сменился _hue
  bool     _fading       = false;
  bool     _lit          = false;
};
