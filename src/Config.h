#pragma once
#include <stddef.h>
#include <stdint.h>

// Значения, помеченные «калибруется», подбираются на железе по трассам (§11).

// ---- Тайминги ----
constexpr uint32_t kTickMs           = 10;
constexpr uint32_t kRenderMs         = 50;
constexpr uint32_t kIdleDimMs        = 20000;
constexpr uint32_t kDimToSleepMs     = 60000;
constexpr uint32_t kBatteryPollMs    = 5000;
constexpr uint32_t kNvsDebounceMs    = 3000;
constexpr uint32_t kSplashMs         = 800;
constexpr uint32_t kMotionActiveMs   = 500;   // «устройство недавно шевелилось»
// Потолок измеренного dt (после сплэша/долгой отрисовки/блокирующей записи
// NVS). Единый для main.cpp и Accel — раньше Accel зажимал свою копию
// отдельно от Context.dtMs, что могло разойтись (final review, Minor).
constexpr uint32_t kAccelMaxDtMs     = 100;

// ---- Детектор тряски (калибруется) ----
constexpr float    kShakeOn          = 0.35f;   // g, по сглаженному excess
constexpr float    kShakeOff         = 0.15f;   // g
constexpr uint32_t kShakeOffMs       = 120;
constexpr uint32_t kRefractoryMs     = 150;
constexpr float    kShakeSat         = 1.5f;    // g, верх нормировки интенсивности
constexpr float    kMotionG          = 0.08f;   // g, порог «устройство шевельнулось»
constexpr float    kFreefallG        = 0.3f;    // g, |a| ниже — свободное падение
constexpr uint32_t kFreefallMs       = 80;
constexpr float    kImpactG          = 3.5f;    // g, excess выше — удар
constexpr uint32_t kFreefallLinkMs   = 500;
// Дебаунс гейта свободного падения, применяется только в State::Shaking
// (в Idle гейт мгновенный — см. ShakeDetector.cpp). Подобран так, чтобы
// отсекать 2-отсчётную впадину синуса в фикстуре feedShake (0.8g/8Hz).
constexpr uint8_t  kFreefallDebounceShaking = 3;
constexpr uint32_t kSuppressMs       = 400;
constexpr uint32_t kExcessTauMs      = 30;
constexpr uint32_t kGravityTauMs     = 300;
// Индекс длинной оси корпуса: 0 = x, 1 = y, 2 = z. Калибруется на железе (§Task 7).
constexpr uint8_t  kTiltAxis         = 0;

// ---- Звук ----
constexpr float    kBaseHz           = 698.46f; // F5
constexpr int8_t   kScale[5]         = {0, 2, 4, 7, 9};
constexpr int      kStepMax          = 9;
constexpr uint8_t  kMaxMasterVolume  = 90;      // из 255; калибруется на слух
constexpr uint8_t  kVolumeQuietPct   = 40;
constexpr size_t   kNoteCapacity     = 32;

enum class VolumeLevel : uint8_t { Quiet, Normal, Mute };

// ---- Экран ----
constexpr int16_t  kBarHeight        = 26;
constexpr int16_t  kScreenWidth      = 135;
constexpr int16_t  kScreenHeight     = 240;
constexpr uint8_t  kMaxBrightness    = 160;     // из 255
constexpr uint8_t  kDimBrightnessPct = 10;

// ---- Батарея ----
constexpr uint8_t  kBattLowPct       = 20;      // жёлтый индикатор
constexpr uint8_t  kBattCritPct      = 10;      // красный индикатор, яркость вдвое
constexpr uint8_t  kBattOffPct       = 5;       // порог выключения
constexpr uint8_t  kBattOffCount     = 5;       // подряд измерений ниже порога
constexpr uint32_t kBattWarnMs       = 2000;    // предупреждение перед выключением

// ---- Погремушка ----
constexpr uint16_t kMaracaNoteMinMs     = 60;
constexpr uint16_t kMaracaNoteMaxMs     = 120;
constexpr uint32_t kMaracaMinIntervalMs = 70;
constexpr uint32_t kMaracaMaxIntervalMs = 220;
constexpr uint32_t kMaracaFadeMs        = 600;
constexpr uint8_t  kMaracaPaletteSize   = 12;
constexpr uint32_t kMaracaLevelTauMs    = 400;  // огибающая яркости, см. расхождение 3
constexpr float    kMaracaValueFloor    = 0.55f;
// Минимальный интервал между сменами HUE полноэкранной вспышки. Без него
// повторная тряска-стоп-тряска может переключать насыщенный цвет на
// частоте ~2-3.7 Гц (кадансуется kRefractoryMs+kShakeOffMs) — это задевает
// границу диапазона фотосенситивного риска (3-30 Гц). Сама вспышка на
// каждую тряску остаётся, просто повторно использует текущий HUE, если
// порог не истёк (final review, Important 4).
constexpr uint32_t kMaracaHueMinIntervalMs = 2000;

// ---- Шкатулка ----
constexpr uint8_t  kWindMax             = 6;
constexpr uint32_t kWindDecayMs         = 4000;
constexpr uint32_t kSweepPeriodFastMs   = 8000;
constexpr uint32_t kSweepPeriodSlowMs   = 12000;
constexpr uint8_t  kMusicBoxPhraseLen   = 12;
constexpr uint16_t kMusicBoxNoteMs      = 180;
constexpr uint16_t kMusicBoxGapMs       = 40;
constexpr float    kMusicBoxSaturation  = 0.5f;
constexpr float    kMusicBoxIdleValue   = 0.4f;

// ---- Пианино ----
constexpr uint8_t  kPianoSteps          = 10;
constexpr float    kPianoMoveDps        = 25.0f;
constexpr uint32_t kPianoReleaseMs      = 250;

// ---- Подтверждение смены режима ----
constexpr int16_t  kConfirmStepLow      = 4;
constexpr int16_t  kConfirmStepHigh     = 7;
constexpr uint16_t kConfirmNoteMs       = 60;
