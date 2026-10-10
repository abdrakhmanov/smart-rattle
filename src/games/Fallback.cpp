#include "Fallback.h"

#include <M5Unified.h>   // lgfx::color565

#include "Config.h"

uint16_t Fallback::nameColor() const {
  return lgfx::color565(0xFF, 0x30, 0x30);
}

void Fallback::onEnter(Context& ctx) {
  _flashUntilMs = 0;
  _step = 0;
  _lit = false;
  ctx.display.clearBody();
}

void Fallback::onExit(Context& ctx) {
  ctx.buzzer.stop();
  ctx.display.clearBody();
}

void Fallback::strike(Context& ctx) {
  const Note n{_step, kMaracaNoteMaxMs, 0};
  ctx.buzzer.play(&n, 1);
  _step = (int16_t)((_step + 2) % (kStepMax + 1));
  _flashUntilMs = ctx.now + 300;
  _lit = false;
}

void Fallback::update(Context& ctx) {
  if (!ctx.renderTick) return;

  const bool shouldLight = ctx.now < _flashUntilMs;
  if (shouldLight == _lit) return;      // одна заливка на событие, не мерцание
  _lit = shouldLight;
  ctx.display.fillBody(shouldLight ? Display::hsv(0.0f, 0.6f, 0.7f) : 0x0000);
}
