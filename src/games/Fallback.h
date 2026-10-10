#pragma once
#include <stdint.h>

#include "Game.h"

// §12: при отказе IMU устройство не выглядит кирпичом — нота играется
// по нажатию A, а причина видна в полосе.
class Fallback : public Game {
 public:
  const char* name()      const override { return "IMU"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

  void strike(Context& ctx);

 private:
  uint32_t _flashUntilMs = 0;
  int16_t  _step         = 0;
  bool     _lit          = false;
};
