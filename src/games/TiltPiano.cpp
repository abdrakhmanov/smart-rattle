#include "TiltPiano.h"

#include <M5Unified.h>   // lgfx::color565

#include "Config.h"

namespace {

constexpr uint16_t kTickColor = 0x8410;   // серые риски квантования

inline int8_t stepOf(float tiltDeg) {
  int s = (int)(tiltDeg / 180.0f * (float)kPianoSteps);
  if (s < 0) s = 0;
  if (s > kPianoSteps - 1) s = kPianoSteps - 1;
  return (int8_t)s;
}

}  // namespace

uint16_t TiltPiano::nameColor() const {
  return lgfx::color565(0x60, 0xE0, 0x90);   // #60E090 зелёный
}

void TiltPiano::onEnter(Context& ctx) {
  _step = -1;
  _prevHeight = 0;
  _silentAtMs = 0;
  _sounding = false;
  ctx.display.clearBody();
}

void TiltPiano::onExit(Context& ctx) {
  ctx.buzzer.stop();
  _sounding = false;
  ctx.display.clearBody();
}

void TiltPiano::drawBar(Context& ctx, int16_t height, uint16_t color, bool full) {
  const int16_t h = ctx.display.bodyHeight();
  const int16_t w = ctx.display.width();

  if (full) {
    // Смена ступени: перекрасить всю полосу и дочистить фон над ней.
    ctx.display.fillBodyRect(0, h - height, w, height, color);
    if (h - height > 0) ctx.display.fillBodyRect(0, 0, w, h - height, 0x0000);
  } else if (height > _prevHeight) {
    ctx.display.fillBodyRect(0, h - height, w, height - _prevHeight, color);
  } else if (height < _prevHeight) {
    ctx.display.fillBodyRect(0, h - _prevHeight, w, _prevHeight - height, 0x0000);
  }

  // Риски на границах квантования: видно, как нота защёлкивается.
  for (uint8_t k = 1; k < kPianoSteps; ++k) {
    const int16_t y = h - (int16_t)((int32_t)h * k / kPianoSteps);
    ctx.display.fillBodyRect(0, y, w, 1, kTickColor);
  }
  _prevHeight = height;
}

void TiltPiano::update(Context& ctx) {
  const float tilt = ctx.accel.tiltAngle();
  const int8_t step = stepOf(tilt);
  const bool stepChanged = (step != _step);
  _step = step;

  // Без этого правила непрерывный тон при любом наклоне изматывал бы
  // взрослых в комнате (§10).
  if (ctx.accel.motionRate() > kPianoMoveDps) {
    ctx.buzzer.glide(Buzzer::pitchOf(step));
    _sounding = true;
    _silentAtMs = ctx.now + kPianoReleaseMs;
  } else if (_sounding && ctx.now >= _silentAtMs) {
    ctx.buzzer.stop();
    _sounding = false;
  }

  if (!ctx.renderTick) return;

  const int16_t height = (int16_t)((float)ctx.display.bodyHeight() * tilt / 180.0f);
  const uint16_t color = Display::hsv(36.0f * (float)step, 1.0f, 1.0f);
  drawBar(ctx, height, color, stepChanged);
}
