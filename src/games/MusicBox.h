#pragma once
#include <stdint.h>

#include "Game.h"

// «Шкатулка»: тряска заводит, фраза играет по кругу, завод убывает,
// музыка и свип оттенка замедляются синхронно.
class MusicBox : public Game {
 public:
  const char* name()      const override { return "Шкатулка"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

 private:
  float    windFraction() const;

  uint8_t  _wind        = 0;
  uint32_t _decayAccMs  = 0;
  float    _hue         = 0.0f;
};
