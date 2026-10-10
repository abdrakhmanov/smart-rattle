#pragma once
#include <stdint.h>

// Вся математика движения. Не зависит от M5Unified, собирается нативно.
// Пороги применяются к сглаженному excess, интенсивность снимается
// с мгновенного (§7).
class ShakeDetector {
 public:
  void reset();

  // ax..az в g, gx..gz в °/с, dtMs — измеренный шаг тика.
  void update(float ax, float ay, float az,
              float gx, float gy, float gz, uint32_t dtMs);

  bool    shakeStarted()   const { return _started; }
  bool    shakeEnded()     const { return _ended; }
  float   startIntensity() const { return _startIntensity; }
  bool    isShaking()      const { return _state == State::Shaking; }
  float   shakeLevel()     const;
  bool    motionSeen()     const { return _motionSeen; }
  float   excess()         const { return _excess; }
  uint8_t stateCode()      const { return (uint8_t)_state; }

 private:
  enum class State : uint8_t { Idle = 0, Shaking = 1 };

  State    _state          = State::Idle;
  bool     _started        = false;
  bool     _ended          = false;
  bool     _motionSeen     = false;
  float    _startIntensity = 0.0f;
  float    _excess         = 0.0f;   // сглаженный
  float    _rawExcess      = 0.0f;   // мгновенный
  float    _gravity[3]     = {0.0f, 0.0f, 1.0f};
  uint32_t _belowOffMs     = 0;      // сколько держится excess < kShakeOff
  uint32_t _refractoryMs   = 0;      // остаток окна после события
  bool     _primed         = false;  // первый тик инициализирует фильтры
};
