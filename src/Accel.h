#pragma once
#include <stdint.h>

#include "ShakeDetector.h"

// Единственный владелец M5.Imu. Отдаёт режимам события и нормализованные
// сигналы, а не отсчёты: физика остаётся здесь и в ShakeDetector.
class Accel {
 public:
  void begin();
  void update(uint32_t nowMs);

  bool isEnabled() const { return _enabled; }

  bool  shakeStarted()   const { return _det.shakeStarted(); }
  bool  shakeEnded()     const { return _det.shakeEnded(); }
  float startIntensity() const { return _det.startIntensity(); }
  bool  isShaking()      const { return _det.isShaking(); }
  float shakeLevel()     const { return _det.shakeLevel(); }
  float tiltAngle()      const { return _det.tiltAngle(); }
  float motionRate()     const { return _det.motionRate(); }

  uint32_t msSinceMotion(uint32_t now) const {
    return now - _lastMotionMs;
  }

  // Для CSV-трасс (MARACA_TRACE).
  float   excess()    const { return _det.excess(); }
  uint8_t stateCode() const { return _det.stateCode(); }

 private:
  ShakeDetector _det;
  bool     _enabled      = false;
  bool     _primed       = false;
  uint32_t _lastUpdateMs = 0;
  uint32_t _lastMotionMs = 0;
};
