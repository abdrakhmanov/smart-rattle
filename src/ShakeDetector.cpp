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
  _motionRate = sqrtf(gx * gx + gy * gy + gz * gz);
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

  // Свободное падение даёт |a| ~ 0, то есть excess ~ 1,0 g — выше kShakeOn.
  // Поэтому события гасятся уже по мгновенному признаку, не дожидаясь
  // подтверждения длительностью kFreefallMs.
  //
  // Контроллер (эскалация по Task 4, второй раунд): дебаунс гейта общий
  // для обоих состояний не может удовлетворить оба требования сразу —
  // у них разная стоимость ошибки:
  //  - Idle: нужен МГНОВЕННЫЙ гейт (0 отсчётов задержки). Падение со стола
  //    начинается из покоя, excess после ФНЧ (tau=kExcessTauMs=30мс)
  //    пересекает kShakeOn уже на 2-м отсчёте (измерено на фикстуре
  //    feedDrop) — любая задержка гейта на этом переходе стреляет
  //    ложной нотой в момент начала падения (именно это отклонение №5
  //    и запрещает).
  //  - Shaking: нужен дебаунс (измерено — порог 3, не 2) отсчётов, иначе
  //    двухотсчётная впадина синуса в фикстуре Task 3 (feedShake,
  //    0.8g/8Hz: mag опускается ниже kFreefallG на 2 подряд отсчёта у
  //    дна каждого периода) каждый раз выбивает автомат в Idle и, после
  //    возврата, немедленно взводит новый shakeStarted — активная тряска
  //    не должна прерываться и перезапускаться такой впадиной.
  // Поэтому гейт зависит от текущего состояния автомата, а не от одной
  // общей константы. kFreefallG и фикстуры Task 3/4 не менялись.
  if (mag < kFreefallG) {
    if (_freefallSampleCount < 0xFFu) ++_freefallSampleCount;
  } else {
    _freefallSampleCount = 0;
  }
  _inFreefall = (_state == State::Idle)
                    ? (_freefallSampleCount >= 1)
                    : (_freefallSampleCount >= kFreefallDebounceShaking);
  if (_inFreefall) {
    _freefallMs += dtMs;
    if (_freefallMs >= kFreefallMs) {
      _sinceFreefallMs = 0;
      _suppressMs      = kSuppressMs;
    }
  } else {
    _freefallMs = 0;
    if (_sinceFreefallMs < 0xFFFFFFFFu - dtMs) _sinceFreefallMs += dtMs;
  }

  // Удар: либо вскоре после падения (гасим и отскок), либо сам по себе —
  // шлепок об стол. В обоих случаях подавление одинаковое (§7).
  if (_rawExcess > kImpactG) {
    _suppressMs = kSuppressMs;
    if (_sinceFreefallMs <= kFreefallLinkMs) {
      // удар связан с падением: окно начинается заново от удара
      _sinceFreefallMs = 0;
    }
  }
  _suppressMs = countdown(_suppressMs, dtMs);

  if (_suppressMs > 0 || _inFreefall) {
    // Внутри окна подавления автомат принудительно возвращается в Idle,
    // чтобы после отскока не остаться в состоянии Shaking.
    _state      = State::Idle;
    _belowOffMs = 0;
    _motionSeen = _excess > kMotionG;
    return;
  }

  // Угол между длинной осью корпуса и вектором гравитации.
  {
    const float gm = sqrtf(_gravity[0] * _gravity[0] +
                           _gravity[1] * _gravity[1] +
                           _gravity[2] * _gravity[2]);
    if (gm > 0.1f) {
      float c = _gravity[kTiltAxis] / gm;
      if (c > 1.0f) c = 1.0f;
      if (c < -1.0f) c = -1.0f;
      _tiltAngle = acosf(c) * 180.0f / 3.14159265f;
    }
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

float ShakeDetector::tiltAngle() const {
  return _tiltAngle;
}
