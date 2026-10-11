#include "MusicBox.h"

#include <M5Unified.h>   // lgfx::color565

#include "Config.h"

namespace {

// Пентатоническая фраза из 12 нот: в пентатонике нет полутоновых и
// тритоновых интервалов, поэтому любая последовательность консонансна (§8).
const Note kPhrase[kMusicBoxPhraseLen] = {
  {0, kMusicBoxNoteMs, kMusicBoxGapMs}, {2, kMusicBoxNoteMs, kMusicBoxGapMs},
  {4, kMusicBoxNoteMs, kMusicBoxGapMs}, {7, kMusicBoxNoteMs, kMusicBoxGapMs},
  {5, kMusicBoxNoteMs, kMusicBoxGapMs}, {4, kMusicBoxNoteMs, kMusicBoxGapMs},
  {2, kMusicBoxNoteMs, kMusicBoxGapMs}, {4, kMusicBoxNoteMs, kMusicBoxGapMs},
  {7, kMusicBoxNoteMs, kMusicBoxGapMs}, {9, kMusicBoxNoteMs, kMusicBoxGapMs},
  {7, kMusicBoxNoteMs, kMusicBoxGapMs}, {4, kMusicBoxNoteMs, kMusicBoxGapMs},
};

}  // namespace

uint16_t MusicBox::nameColor() const {
  return lgfx::color565(0xB0, 0xA0, 0xFF);   // #B0A0FF лавандовый
}

float MusicBox::windFraction() const {
  return (float)_wind / (float)kWindMax;
}

void MusicBox::onEnter(Context& ctx) {
  _wind = 0;
  _decayAccMs = 0;
  _hue = 0.0f;
  ctx.buzzer.setTempoScale(1.0f);
  ctx.display.clearBody();
}

void MusicBox::onExit(Context& ctx) {
  ctx.buzzer.stop();
  ctx.buzzer.setTempoScale(1.0f);
  ctx.display.clearBody();
}

void MusicBox::update(Context& ctx) {
  if (ctx.accel.shakeStarted() && _wind < kWindMax) {
    ++_wind;
  }

  _decayAccMs += ctx.dtMs;
  while (_decayAccMs >= kWindDecayMs) {
    _decayAccMs -= kWindDecayMs;
    if (_wind > 0) --_wind;
  }

  const float frac = windFraction();

  if (_wind > 0) {
    // Чем меньше завод, тем медленнее музыка: множитель растёт по мере
    // разрядки (см. «Расхождения», п. 2).
    ctx.buzzer.setTempoScale(1.4f - 0.4f * frac);
    if (ctx.buzzer.pending() == 0) {
      for (uint8_t i = 0; i < kMusicBoxPhraseLen; ++i) ctx.buzzer.push(kPhrase[i]);
    }
  }

  // Свип оттенка: 8 с на круг при полном заводе, 12 с при нулевом —
  // 0,08–0,12 Гц, с запасом безопасно по мерцанию (§10).
  const float period = (float)kSweepPeriodFastMs
                     + ((float)kSweepPeriodSlowMs - (float)kSweepPeriodFastMs)
                       * (1.0f - frac);
  _hue += 360.0f * (float)ctx.dtMs / period;
  while (_hue >= 360.0f) _hue -= 360.0f;

  if (!ctx.renderTick) return;
  const float value = (_wind > 0) ? 1.0f : kMusicBoxIdleValue;
  ctx.display.fillBody(Display::hsv(_hue, kMusicBoxSaturation, value));
}
