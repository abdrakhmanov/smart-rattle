#pragma once
#include <math.h>
#include <stdint.h>
#include "ShakeDetector.h"

struct EventCounts {
  int started = 0;
  int ended   = 0;
  float firstIntensity = -1.0f;
};

// Покой: гравитация по оси z, слабый шум.
inline void feedRest(ShakeDetector& d, EventCounts& c, uint32_t ms, uint32_t dtMs = 10) {
  for (uint32_t t = 0; t < ms; t += dtMs) {
    const float n = 0.01f * sinf(t * 0.7f);
    d.update(n, -n, 1.0f + n, 0.5f, -0.5f, 0.2f, dtMs);
    if (d.shakeStarted()) { if (c.firstIntensity < 0) c.firstIntensity = d.startIntensity(); ++c.started; }
    if (d.shakeEnded())   ++c.ended;
  }
}

// Тряска: синус амплитудой ampG по оси z поверх гравитации.
inline void feedShake(ShakeDetector& d, EventCounts& c, float ampG, float freqHz,
                      uint32_t ms, uint32_t dtMs = 10) {
  for (uint32_t t = 0; t < ms; t += dtMs) {
    const float phase = 2.0f * 3.14159265f * freqHz * (t / 1000.0f);
    const float a = ampG * sinf(phase);
    d.update(0.0f, 0.0f, 1.0f + a, 400.0f * sinf(phase), 0.0f, 0.0f, dtMs);
    if (d.shakeStarted()) { if (c.firstIntensity < 0) c.firstIntensity = d.startIntensity(); ++c.started; }
    if (d.shakeEnded())   ++c.ended;
  }
}

// Постоянное превышение: excess == ampG на каждом отсчёте. Нужен там, где
// важна точная величина и повторяемость, а не реалистичность сигнала.
inline void feedConstant(ShakeDetector& d, EventCounts& c, float ampG,
                         uint32_t ms, uint32_t dtMs = 10) {
  for (uint32_t t = 0; t < ms; t += dtMs) {
    d.update(0.0f, 0.0f, 1.0f + ampG, 0.0f, 0.0f, 0.0f, dtMs);
    if (d.shakeStarted()) { if (c.firstIntensity < 0) c.firstIntensity = d.startIntensity(); ++c.started; }
    if (d.shakeEnded())   ++c.ended;
  }
}
