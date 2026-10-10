#include "Maraca.h"

#include <M5Unified.h>   // Arduino random() и lgfx::color565

#include "Config.h"

namespace {

// Честно случайный RGB даёт преимущественно грязно-коричневое: берём
// 12 насыщенных оттенков (§10).
inline float hueOf(uint8_t index) {
  return 360.0f / (float)kMaracaPaletteSize * (float)index;
}

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

}  // namespace

uint16_t Maraca::nameColor() const {
  return lgfx::color565(0xFF, 0xB0, 0x40);   // #FFB040 янтарный
}

void Maraca::onEnter(Context& ctx) {
  _paletteIndex = -1;
  _levelEnv = 0.0f;
  _nextNoteMs = 0;
  _fading = false;
  _lit = false;
  ctx.display.clearBody();
}

void Maraca::onExit(Context& ctx) {
  ctx.buzzer.stop();
  ctx.display.clearBody();
}

void Maraca::startBurst(Context& ctx) {
  // Повторный быстрый шейк-стоп-шейк не должен менять насыщенный цвет на
  // каждую вспышку — это могло стробить в районе 2-3,7 Гц (final review,
  // Important 4). Сама вспышка/нота всё равно происходят каждый раз.
  if (_paletteIndex < 0 || (ctx.now - _lastHueMs) >= kMaracaHueMinIntervalMs) {
    int8_t pick = (int8_t)random(kMaracaPaletteSize);
    if (pick == _paletteIndex) {
      pick = (int8_t)((pick + 1) % kMaracaPaletteSize);
    }
    _paletteIndex = pick;
    _hue = hueOf((uint8_t)pick);
    _lastHueMs = ctx.now;
  }

  const float intensity = ctx.accel.startIntensity();
  const uint16_t dur = (uint16_t)lerpf((float)kMaracaNoteMinMs,
                                      (float)kMaracaNoteMaxMs, intensity);
  const Note first{(int16_t)random(kStepMax + 1), dur, 0};
  ctx.buzzer.play(&first, 1);

  _levelEnv = ctx.accel.shakeLevel();
  _fading = false;
  _lit = true;
  _nextNoteMs = ctx.now + kMaracaMaxIntervalMs;
  ctx.display.fillBody(Display::hsv(_hue, 1.0f, 1.0f));
}

void Maraca::update(Context& ctx) {
  if (ctx.accel.shakeStarted()) {
    startBurst(ctx);
  }

  if (ctx.accel.isShaking()) {
    // Огибающая, а не мгновенный уровень: модуляция яркости обязана быть
    // медленнее 0,5 Гц (§9), иначе экран стробит.
    const float alpha = (float)ctx.dtMs / (float)(kMaracaLevelTauMs + ctx.dtMs);
    _levelEnv += alpha * (ctx.accel.shakeLevel() - _levelEnv);

    if (ctx.now >= _nextNoteMs) {
      const uint32_t interval = (uint32_t)lerpf((float)kMaracaMaxIntervalMs,
                                               (float)kMaracaMinIntervalMs,
                                               ctx.accel.shakeLevel());
      ctx.buzzer.push({(int16_t)random(kStepMax + 1), kMaracaNoteMinMs, 0});
      _nextNoteMs = ctx.now + interval;
    }
  }

  if (ctx.accel.shakeEnded()) {
    _fadeStartMs = ctx.now;
    _fading = true;
  }

  if (!ctx.renderTick) return;

  if (_fading) {
    const uint32_t elapsed = ctx.now - _fadeStartMs;
    if (elapsed >= kMaracaFadeMs) {
      _fading = false;
      _lit = false;
      ctx.display.clearBody();
    } else {
      const float k = 1.0f - (float)elapsed / (float)kMaracaFadeMs;
      ctx.display.fillBody(Display::hsv(_hue, 1.0f, k));
    }
    return;
  }

  if (_lit && ctx.accel.isShaking()) {
    const float value = lerpf(kMaracaValueFloor, 1.0f, _levelEnv);
    ctx.display.fillBody(Display::hsv(_hue, 1.0f, value));
  }
}
