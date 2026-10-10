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

// Падение со стола: покой → свободное падение → удар → отскок → покой.
inline void feedDrop(ShakeDetector& d, EventCounts& c, uint32_t dtMs = 10) {
  feedRest(d, c, 400, dtMs);
  // свободное падение 150 мс: |a| ~ 0
  for (uint32_t t = 0; t < 150; t += dtMs) {
    d.update(0.02f, -0.01f, 0.03f, 20.0f, 10.0f, 5.0f, dtMs);
    if (d.shakeStarted()) ++c.started;
    if (d.shakeEnded())   ++c.ended;
  }
  // удар 20 мс: 6 g
  for (uint32_t t = 0; t < 20; t += dtMs) {
    d.update(0.0f, 0.0f, 6.0f, 800.0f, 400.0f, 200.0f, dtMs);
    if (d.shakeStarted()) ++c.started;
    if (d.shakeEnded())   ++c.ended;
  }
  // отскок 200 мс: затухающие колебания ±1,2 g
  for (uint32_t t = 0; t < 200; t += dtMs) {
    const float env = 1.2f * (1.0f - (float)t / 200.0f);
    const float a = env * sinf(2.0f * 3.14159265f * 14.0f * (t / 1000.0f));
    d.update(0.0f, 0.0f, 1.0f + a, 100.0f, 0.0f, 0.0f, dtMs);
    if (d.shakeStarted()) ++c.started;
    if (d.shakeEnded())   ++c.ended;
  }
  feedRest(d, c, 600, dtMs);
}

// Шлепок об стол: покой → одиночный пик 4 g → покой. Падения не было.
inline void feedSlap(ShakeDetector& d, EventCounts& c, uint32_t dtMs = 10) {
  feedRest(d, c, 400, dtMs);
  for (uint32_t t = 0; t < 20; t += dtMs) {
    d.update(0.0f, 0.0f, 5.5f, 300.0f, 100.0f, 50.0f, dtMs);
    if (d.shakeStarted()) ++c.started;
    if (d.shakeEnded())   ++c.ended;
  }
  feedRest(d, c, 800, dtMs);
}

// Статический наклон: гравитация делится между осью X (длинная) и Z.
inline void feedTilt(ShakeDetector& d, EventCounts& c, float degrees,
                     uint32_t ms = 2000, uint32_t dtMs = 10) {
  const float rad = degrees * 3.14159265f / 180.0f;
  for (uint32_t t = 0; t < ms; t += dtMs) {
    d.update(cosf(rad), 0.0f, sinf(rad), 0.0f, 0.0f, 0.0f, dtMs);
    if (d.shakeStarted()) ++c.started;
    if (d.shakeEnded())   ++c.ended;
  }
}
