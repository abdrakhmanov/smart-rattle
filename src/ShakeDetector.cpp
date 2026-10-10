#include "ShakeDetector.h"

#include <math.h>

#include "Config.h"

namespace {

// Однополюсный ФНЧ по измеренному dt: alpha = dt / (tau + dt).
inline void lowpass(float& y, float x, uint32_t dtMs, uint32_t tauMs) {
  const float alpha = (float)dtMs / (float)(tauMs + dtMs);
  y += alpha * (x - y);
}

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Нормировка g-значения в 0..1 по диапазону kShakeOn..kShakeSat.
inline float normalizeExcess(float g) {
  return clamp01((g - kShakeOn) / (kShakeSat - kShakeOn));
}

inline uint32_t countdown(uint32_t value, uint32_t dtMs) {
  return value > dtMs ? value - dtMs : 0;
}

}  // namespace

void ShakeDetector::reset() {
  *this = ShakeDetector();
}

void ShakeDetector::update(float ax, float ay, float az,
                           float gx, float gy, float gz, uint32_t dtMs) {
  (void)gx; (void)gy; (void)gz;  // используются в Task 4
  if (dtMs == 0) dtMs = 1;

  _started = false;
  _ended   = false;

  const float mag = sqrtf(ax * ax + ay * ay + az * az);
  _rawExcess = fabsf(mag - 1.0f);

  if (!_primed) {
    _primed  = true;
    _excess  = _rawExcess;
    _gravity[0] = ax; _gravity[1] = ay; _gravity[2] = az;
  } else {
    lowpass(_excess, _rawExcess, dtMs, kExcessTauMs);
    lowpass(_gravity[0], ax, dtMs, kGravityTauMs);
    lowpass(_gravity[1], ay, dtMs, kGravityTauMs);
    lowpass(_gravity[2], az, dtMs, kGravityTauMs);
  }

  _motionSeen   = _excess > kMotionG;
  _refractoryMs = countdown(_refractoryMs, dtMs);

  if (_state == State::Idle) {
    _belowOffMs = 0;
    if (_excess > kShakeOn && _refractoryMs == 0) {
      _state          = State::Shaking;
      _startIntensity = normalizeExcess(_rawExcess);
      _started        = true;
      _refractoryMs   = kRefractoryMs;
    }
    return;
  }

  // State::Shaking
  if (_excess < kShakeOff) {
    _belowOffMs += dtMs;
  } else {
    _belowOffMs = 0;
  }
  if (_belowOffMs >= kShakeOffMs) {
    _state        = State::Idle;
    _ended        = true;
    _belowOffMs   = 0;
    _refractoryMs = kRefractoryMs;
  }
}

float ShakeDetector::shakeLevel() const {
  return normalizeExcess(_excess);
}
