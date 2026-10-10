# Детская погремушка на M5StickC Plus2 — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Превратить M5StickC Plus2 в детскую музыкальную игрушку: движение даёт звук и цвет, три режима, безопасные потолки громкости и яркости, автосон.

**Architecture:** Один поток, один `loop()` на 100 Гц, отрисовка throttled до 20 Гц. Вся физика движения вынесена в `ShakeDetector` без зависимости от M5Unified и тестируется нативно; `Accel`, `Buzzer`, `Display` — единственные владельцы `M5.Imu`, `M5.Speaker`, `M5.Display`. Режимы получают `Context` (accel/buzzer/display) и не видят `Power`, который единолично владеет политикой сна.

**Tech Stack:** PlatformIO (платформа pioarduino espressif32), framework arduino, `m5stack/M5Unified@^0.2.21` (тянет M5GFX с efont), Unity для нативных тестов, локальное описание платы `boards/m5stack-stickc-plus2.json`.

**Spec:** [docs/superpowers/specs/2026-10-10-stick-maraca-design.md](../specs/2026-10-10-stick-maraca-design.md)

## Global Constraints

- Прямое обращение к `M5.Imu` — только в `src/Accel.cpp`; к `M5.Speaker` — только в `src/Buzzer.cpp`; к `M5.Display` — только в `src/Display.cpp`. Исключения: `M5.begin()`, `M5.update()`, `M5.delay()`, кнопки `M5.BtnA/BtnB` и `M5.Power.*` в `main.cpp` и `src/Power.cpp`.
- `M5.Speaker.setVolume()` вызывается ровно в одном месте — `Buzzer::setVolumeLevel()`. Потолок `kMaxMasterVolume = 90`.
- `M5.Display.setBrightness()` вызывается ровно в одном месте — `Display::setBrightness()`, с зажимом по `kMaxBrightness = 160`.
- Все фильтры и детекторы считают по измеренному `dtMs`, никогда по счётчику итераций.
- Динамическая память не используется: ни `new`, ни `malloc`, ни `std::vector`, ни `std::function` в прошивке. Экземпляры режимов статические.
- Тело экрана меняет заливку либо один раз на дискретное событие, либо плавным свипом медленнее 0,5 Гц. Это обязательный пункт ревью каждого режима.
- Строки интерфейса в UTF-8, русские. Шрифт — `lgfx::v1::fonts::efontCN_14` (проверено: содержит блок кириллицы U+0400..U+04FF, занимает 262 КБ флеша).
- Прошивка должна укладываться в APP-раздел 3 342 336 байт.
- PSRAM не включается.
- Целевое окружение сборки — `m5stack-stickc-plus2`, нативных тестов — `native`.
- Шаги с пометкой **[железо]** требуют подключённого по USB устройства. Если устройства нет — шаг объявляется заблокированным и об этом сообщается явно; молча пропускать нельзя.

## Состав файлов

| Файл | Ответственность | Задача |
|---|---|---|
| `boards/m5stack-stickc-plus2.json` | описание платы (пинаут, 8 МБ флеша) | 1 |
| `platformio.ini` | окружения `m5stack-stickc-plus2` и `native` | 1 |
| `src/Config.h` | все константы, `enum class VolumeLevel` | 1 |
| `src/ShakeDetector.{h,cpp}` | фильтры, excess, автомат тряски, падения, наклон | 3, 4 |
| `src/Buzzer.{h,cpp}` | пентатоника, секвенсор нот, громкость, glide | 5, 6 |
| `src/Accel.{h,cpp}` | опрос `M5.Imu`, CSV-трассы | 7 |
| `src/Display.{h,cpp}` | полоса, индикаторы, тело экрана | 8 |
| `src/Game.h` | интерфейс `Game`, структура `Context` | 9 |
| `src/games/Maraca.{h,cpp}` | режим «Погремушка» | 9 |
| `src/main.cpp` | setup/loop, тик, композиция | 9, 10, 11, 14 |
| `src/GameManager.{h,cpp}` | переключение режимов, громкость, сохранение | 10 |
| `src/Power.{h,cpp}` | HOLD, простой, deep sleep, батарея | 11 |
| `src/games/MusicBox.{h,cpp}` | режим «Шкатулка» | 12 |
| `src/games/TiltPiano.{h,cpp}` | режим «Пианино» | 13 |
| `src/games/Fallback.{h,cpp}` | аварийный режим при отсутствии IMU | 14 |
| `test/test_config/` | инварианты констант | 1 |
| `test/test_shake/` | тесты детектора на синтетике и трассах | 3, 4 |
| `test/test_music/` | тесты пентатоники и секвенсора | 5, 6 |
| `test/traces/` | CSV-трассы с железа (фикстуры) | 7 |
| `docs/hw-notes.md` | результаты проверок на железе | 2, 14 |

## Расхождения со спецификацией (решены здесь, осознанно)

1. **§8, диапазон 700–3000 Гц.** `pitchOf(0) = kBaseHz = 698,46` Гц — на 1,5 Гц ниже заявленной нижней границы. Тест проверяет `[698, 3000]`; 700 Гц трактуется как округление в тексте спецификации, а не как требование.
2. **§10, «Шкатулка», `setTempoScale`.** Формула `0.6 + 0.4 * wind / kWindMax` противоречит собственному тексту спецификации («по мере раскручивания музыка замедляется») и таблице периодов свипа (8 с при полном заводе, 12 с при нулевом): она ускоряет музыку по мере разрядки завода. Реализуется `1.4 - 0.4 * wind / kWindMax` (1,0 при полном заводе, 1,33 при `wind == 1`), что согласуется и с текстом, и с периодами свипа.
3. **§10, «Погремушка», пульсация яркости против §9.** Модуляция яркости напрямую по `shakeLevel()` на 20 Гц нарушила бы инвариант 0,5 Гц. Реализуется огибающая: `shakeLevel()` сглаживается в режиме фильтром с `kMaracaLevelTauMs = 400` мс, а значение (`v` в HSV) ограничено снизу `kMaracaValueFloor = 0.55`. Это «дыхание», а не мерцание.
4. **§7, refractory-окно.** Применяется только к переходу `Idle → Shaking`. Окно `kRefractoryMs = 150` длиннее `kShakeOffMs = 120`, поэтому блокировка им события `ShakeEnded` потеряла бы затухание в «Погремушке» и рассогласовала бы состояние детектора с наблюдаемым.
5. **§7, свободное падение.** При свободном падении `|a| ≈ 0`, значит `excess = |0 − 1| = 1,0 g` — выше `kShakeOn`. Поэтому события гасятся уже по мгновенному признаку `|a| < kFreefallG` (а не только после подтверждения падения длительностью `kFreefallMs`), иначе падение со стола выстреливало бы нотой на первом же отсчёте.
6. **§9, интерфейс `Display`.** Добавлены `drawBodyMessage()` (нужен для сплэша §6 и предупреждения о разряде §6) и `setFault()` (нужен для красного маркера `IMU` §12).
7. **§13, константы.** Добавлены `kTiltAxis` (какая ось корпуса длинная — калибруется на железе), `kMotionActiveMs` (как долго после последнего превышения `kMotionG` устройство считается активным), пороги батареи (`kBattLowPct`, `kBattCritPct`, `kBattOffPct`, `kBattOffCount`, `kBattWarnMs`) и константы «Шкатулки»/«Погремушки», которые в §10 заданы текстом, а не таблицей.

---

### Task 1: Сборка под Plus2, `Config.h`, нативное окружение

**Files:**
- Create: `boards/m5stack-stickc-plus2.json`
- Create: `src/Config.h`
- Create: `test/test_config/test_config.cpp`
- Modify: `platformio.ini` (полная замена)
- Delete: `src/Accel.h` (пустая заготовка), каталог `.pio/`

**Interfaces:**
- Consumes: ничего.
- Produces: все константы из `src/Config.h` (имена по §13), `enum class VolumeLevel : uint8_t { Quiet, Normal, Mute }`. Окружения сборки `m5stack-stickc-plus2` и `native`.

- [ ] **Step 1: Написать падающий тест инвариантов констант**

Создать `test/test_config/test_config.cpp`:

```cpp
#include <unity.h>
#include "Config.h"

void test_shake_thresholds_ordered() {
  TEST_ASSERT_TRUE(kShakeOff < kShakeOn);
  TEST_ASSERT_TRUE(kShakeOn < kShakeSat);
  TEST_ASSERT_TRUE(kShakeSat < kImpactG);
  TEST_ASSERT_TRUE(kMotionG < kShakeOff);
  TEST_ASSERT_TRUE(kFreefallG < 1.0f);
}

void test_render_is_multiple_of_tick() {
  TEST_ASSERT_EQUAL_UINT32(0, kRenderMs % kTickMs);
  TEST_ASSERT_TRUE(kRenderMs >= kTickMs);
}

void test_sleep_stages_ordered() {
  TEST_ASSERT_TRUE(kIdleDimMs < kDimToSleepMs);
  TEST_ASSERT_TRUE(kMotionActiveMs < kIdleDimMs);
}

void test_volume_and_brightness_ceilings() {
  TEST_ASSERT_TRUE(kMaxMasterVolume <= 255);
  TEST_ASSERT_TRUE(kVolumeQuietPct <= 100);
  TEST_ASSERT_TRUE(kMaxBrightness <= 255);
  TEST_ASSERT_TRUE(kDimBrightnessPct <= 100);
}

void test_scale_and_steps() {
  TEST_ASSERT_EQUAL_INT(9, kStepMax);
  TEST_ASSERT_EQUAL_INT(0, kScale[0]);
  TEST_ASSERT_EQUAL_INT(9, kScale[4]);
  TEST_ASSERT_TRUE(kTiltAxis < 3);
}

void test_maraca_intervals_ordered() {
  TEST_ASSERT_TRUE(kMaracaMinIntervalMs < kMaracaMaxIntervalMs);
  TEST_ASSERT_TRUE(kMaracaNoteMinMs < kMaracaNoteMaxMs);
  TEST_ASSERT_TRUE(kSweepPeriodFastMs < kSweepPeriodSlowMs);
}

void test_battery_thresholds_ordered() {
  TEST_ASSERT_TRUE(kBattOffPct < kBattCritPct);
  TEST_ASSERT_TRUE(kBattCritPct < kBattLowPct);
  TEST_ASSERT_TRUE(kBattOffCount >= 2);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_shake_thresholds_ordered);
  RUN_TEST(test_render_is_multiple_of_tick);
  RUN_TEST(test_sleep_stages_ordered);
  RUN_TEST(test_volume_and_brightness_ceilings);
  RUN_TEST(test_scale_and_steps);
  RUN_TEST(test_maraca_intervals_ordered);
  RUN_TEST(test_battery_thresholds_ordered);
  return UNITY_END();
}
```

- [ ] **Step 2: Заменить `platformio.ini`**

Полное содержимое файла:

```ini
; PlatformIO Project Configuration File
; M5StickC Plus2 — детская погремушка.

[env:m5stack-stickc-plus2]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = m5stack-stickc-plus2
framework = arduino
board_build.partitions = default_8MB.csv
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
upload_speed = 1500000
lib_deps = m5stack/M5Unified@^0.2.21
build_flags =
	-std=gnu++17
	-D CORE_DEBUG_LEVEL=3
build_unflags = -std=gnu++11
; Нативные наборы тестов в этом окружении не собираются.
test_ignore = test_*

[env:native]
platform = native
test_framework = unity
test_build_src = yes
build_src_filter = -<*>
build_flags =
	-std=gnu++17
	-I src
	-D MARACA_NATIVE=1
```

- [ ] **Step 3: Создать описание платы**

Создать `boards/m5stack-stickc-plus2.json` (значения из `framework-arduinoespressif32/boards.txt`, секция `m5stack_stickc_plus2`):

```json
{
  "build": {
    "core": "esp32",
    "extra_flags": "-DARDUINO_M5STACK_STICKC_PLUS2",
    "f_cpu": "240000000L",
    "f_flash": "80000000L",
    "flash_mode": "dio",
    "mcu": "esp32",
    "variant": "m5stack_stickc_plus2",
    "partitions": "default_8MB.csv"
  },
  "connectivity": [
    "wifi",
    "bluetooth"
  ],
  "debug": {
    "openocd_board": "esp-wroom-32.cfg"
  },
  "frameworks": [
    "arduino",
    "espidf"
  ],
  "name": "M5StickC Plus2",
  "upload": {
    "flash_size": "8MB",
    "maximum_ram_size": 327680,
    "maximum_size": 3342336,
    "require_upload_port": true,
    "speed": 1500000
  },
  "url": "https://docs.m5stack.com/en/core/M5StickC%20PLUS2",
  "vendor": "M5Stack"
}
```

- [ ] **Step 4: Прогнать тест и убедиться, что он падает**

```bash
rm -rf .pio && pio test -e native
```

Ожидается: ошибка компиляции `Config.h: No such file or directory`.

- [ ] **Step 5: Создать `src/Config.h`**

```cpp
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
```

- [ ] **Step 6: Прогнать тест и убедиться, что он проходит**

```bash
pio test -e native
```

Ожидается: 7 тестов PASSED.

- [ ] **Step 7: Удалить заготовку `src/Accel.h` и собрать прошивку**

```bash
rm -f src/Accel.h && pio run -e m5stack-stickc-plus2
```

Ожидается: `SUCCESS`, в отчёте `Flash: [ ] ... (used ... bytes from 3342336 bytes)`. Если в отчёте фигурирует 4194304 или 1310720 — подхватилось не то описание платы, разбираться до продолжения.

- [ ] **Step 8: Коммит**

```bash
git add .gitignore .vscode boards platformio.ini src include lib test docs
git commit -m "build: target M5StickC Plus2, add Config.h and native test env"
```

---

### Task 2: Спайк deep sleep и удержания HOLD на железе

Главный риск спецификации (§14): если пин HOLD не удерживается во сне, устройство снимет с себя питание, а симптом будет выглядеть как «не просыпается по кнопке A». Проверяется до того, как поверх ляжет остальной код.

**Files:**
- Create: `src/sleep_probe.cpp` (временный, удаляется в Step 6)
- Create: `docs/hw-notes.md`
- Modify: `src/main.cpp` (временно переименовывается, возвращается в Step 6)

**Interfaces:**
- Consumes: окружение сборки из Task 1.
- Produces: запись в `docs/hw-notes.md` с ответом «удерживает / не удерживает» и тем, какая последовательность засыпания принята в `Power` (Task 11).

- [ ] **Step 1: Убрать штатный `main.cpp` из сборки и написать пробник**

```bash
git mv src/main.cpp src/main.cpp.bak
```

Создать `src/sleep_probe.cpp`:

```cpp
#include <M5Unified.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

RTC_DATA_ATTR static uint32_t s_wakeCount = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);
  M5.Display.setBrightness(160);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(4, 4);

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    ++s_wakeCount;
  } else {
    s_wakeCount = 0;
  }
  M5.Display.printf("cause=%d\nwakes=%u\nhold pin=%d\n",
                    (int)cause, (unsigned)s_wakeCount,
                    (int)M5.getPin(m5::pin_name_t::power_hold));
  Serial.printf("cause=%d wakes=%u hold=%d\n",
                (int)cause, (unsigned)s_wakeCount,
                (int)M5.getPin(m5::pin_name_t::power_hold));
}

void loop() {
  M5.update();
  static uint32_t started = millis();
  if (millis() - started < 5000) {
    M5.delay(10);
    return;
  }

  Serial.println("going to deep sleep");
  Serial.flush();
  M5.Speaker.stop();
  for (int b = 160; b >= 0; b -= 16) {
    M5.Display.setBrightness((uint8_t)(b < 0 ? 0 : b));
    delay(20);
  }
  M5.Display.sleep();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0);
  const gpio_num_t hold = (gpio_num_t)M5.getPin(m5::pin_name_t::power_hold);
  gpio_hold_en(hold);
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
}
```

- [ ] **Step 2: Собрать и прошить [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t upload
```

Ожидается: `SUCCESS`. Если портов несколько, добавить `--upload-port`.

- [ ] **Step 3: Проверить засыпание и пробуждение [железо]**

```bash
pio device monitor -e m5stack-stickc-plus2
```

Отсоединить USB (питание от батареи), затем наблюдать:

1. Экран горит ~5 с, печатает `cause=0 wakes=0`.
2. Экран гаснет, устройство замолкает.
3. Светодиод/дисплей не включаются сами.
4. Нажать кнопку A (лицевая «M5»). Устройство должно подняться и показать `cause=2 wakes=1` (`2` = `ESP_SLEEP_WAKEUP_EXT0`).
5. Повторить ещё дважды: `wakes` должен расти (2, 3) — значит RTC-память сохранилась, то есть это был сон, а не полное выключение.

**Если `wakes` сбрасывается в 0 или устройство не поднимается по A:** питание снималось. Зафиксировать это в заметках и в Task 11 дополнительно вызвать `gpio_hold_en` **до** `M5.Display.sleep()`, а также проверить вариант с `rtc_gpio_hold_en`. Решение фиксируется здесь, а не в коде `Power`.

- [ ] **Step 4: Проверить, что пробуждение не требует удержания > 2 с [железо]**

Короткое нажатие A (менее 0,5 с) должно поднимать устройство. Если нет — записать фактическое поведение в заметки.

- [ ] **Step 5: Записать результат**

Создать `docs/hw-notes.md`:

```markdown
# Проверки на железе

## Deep sleep и HOLD (Task 2, спайк §14)

Дата проверки: <YYYY-MM-DD>
Прошивка: `src/sleep_probe.cpp`

| Проверка | Результат |
|---|---|
| `M5.getPin(power_hold)` вернул | <значение> |
| Экран гаснет, устройство замолкает | <да/нет> |
| Пробуждение коротким нажатием A | <да/нет> |
| `esp_sleep_get_wakeup_cause()` после A | <значение> |
| `RTC_DATA_ATTR` счётчик растёт между снами | <да/нет> |

Вывод: <питание удерживается явными gpio_hold_en + gpio_deep_sleep_hold_en /
требуется дополнительно ...>

Принятая последовательность засыпания для `Power::enterDeepSleep()`: <описание>.
```

- [ ] **Step 6: Убрать пробник, вернуть `main.cpp`**

```bash
rm src/sleep_probe.cpp && git mv src/main.cpp.bak src/main.cpp && pio run -e m5stack-stickc-plus2
```

Ожидается: `SUCCESS`.

- [ ] **Step 7: Коммит**

```bash
git add docs/hw-notes.md src/main.cpp
git commit -m "docs: record deep sleep and HOLD probe results on hardware"
```

---

### Task 3: `ShakeDetector` — фильтры, excess и автомат тряски

**Files:**
- Create: `src/ShakeDetector.h`, `src/ShakeDetector.cpp`
- Create: `test/test_shake/signals.h`, `test/test_shake/test_shake.cpp`
- Modify: `platformio.ini` (`build_src_filter` окружения `native`)

**Interfaces:**
- Consumes: константы из `src/Config.h` (Task 1).
- Produces:
  ```cpp
  class ShakeDetector {
   public:
    void    reset();
    void    update(float ax, float ay, float az,
                   float gx, float gy, float gz, uint32_t dtMs);
    bool    shakeStarted() const;   // событие текущего тика
    bool    shakeEnded()   const;   // событие текущего тика
    float   startIntensity() const; // 0..1, снимается на фронте
    bool    isShaking()    const;
    float   shakeLevel()   const;   // 0..1
    bool    motionSeen()   const;   // excess превысил kMotionG в этом тике
    float   excess()       const;   // сглаженный, g
    uint8_t stateCode()    const;   // 0 = Idle, 1 = Shaking
  };
  ```
  (`tiltAngle()`, `motionRate()`, `suppressed()` добавляются в Task 4.)

- [ ] **Step 1: Написать генераторы сигналов для тестов**

Создать `test/test_shake/signals.h`:

```cpp
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
```

- [ ] **Step 2: Написать падающие тесты ядра детектора**

Создать `test/test_shake/test_shake.cpp`:

```cpp
#include <unity.h>
#include "Config.h"
#include "signals.h"

void test_rest_yields_no_events() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedRest(d, c, 3000);
  TEST_ASSERT_EQUAL_INT(0, c.started);
  TEST_ASSERT_EQUAL_INT(0, c.ended);
  TEST_ASSERT_FALSE(d.isShaking());
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, d.excess());
}

void test_shake_starts_once_and_holds() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedRest(d, c, 500);
  feedShake(d, c, 0.8f, 8.0f, 2000);
  TEST_ASSERT_TRUE(c.started >= 1);
  TEST_ASSERT_TRUE(c.started <= 3);
  TEST_ASSERT_TRUE(d.isShaking());
  TEST_ASSERT_EQUAL_INT(0, c.ended);
}

void test_shake_ends_after_quiet() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedRest(d, c, 300);
  feedShake(d, c, 0.8f, 8.0f, 1000);
  feedRest(d, c, 1000);
  TEST_ASSERT_EQUAL_INT(1, c.ended);
  TEST_ASSERT_FALSE(d.isShaking());
}

void test_weak_motion_below_threshold_is_ignored() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedShake(d, c, 0.10f, 6.0f, 2000);
  TEST_ASSERT_EQUAL_INT(0, c.started);
}

// Одна энергичная тряска не должна выстрелить десятком событий: после
// каждого события действует окно kRefractoryMs.
void test_refractory_blocks_immediate_retrigger() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedRest(d, c, 300);
  feedConstant(d, c, 0.8f, 60);     // старт примерно на 20-й мс вспышки
  feedRest(d, c, 180);              // тишины хватает ровно на ShakeEnded
  TEST_ASSERT_EQUAL_INT(1, c.started);
  TEST_ASSERT_EQUAL_INT(1, c.ended);

  feedConstant(d, c, 0.8f, 100);    // целиком внутри окна kRefractoryMs
  TEST_ASSERT_EQUAL_INT(1, c.started);

  feedConstant(d, c, 0.8f, 200);    // окно истекло — событие снова возможно
  TEST_ASSERT_EQUAL_INT(2, c.started);
}

void test_start_intensity_monotonic_in_amplitude() {
  float prev = -1.0f;
  const float amps[3] = {0.5f, 1.0f, 1.5f};
  for (int i = 0; i < 3; ++i) {
    ShakeDetector d; d.reset();
    EventCounts c;
    feedRest(d, c, 300);
    feedConstant(d, c, amps[i], 200);
    TEST_ASSERT_TRUE(c.started >= 1);
    TEST_ASSERT_TRUE(c.firstIntensity >= 0.0f);
    TEST_ASSERT_TRUE(c.firstIntensity <= 1.0f);
    TEST_ASSERT_TRUE(c.firstIntensity >= prev);
    prev = c.firstIntensity;
  }
}

void test_shake_level_grows_with_amplitude() {
  ShakeDetector weak; weak.reset();
  ShakeDetector strong; strong.reset();
  EventCounts c1, c2;
  feedShake(weak, c1, 0.5f, 8.0f, 1500);
  feedShake(strong, c2, 1.4f, 8.0f, 1500);
  TEST_ASSERT_TRUE(strong.shakeLevel() > weak.shakeLevel());
  TEST_ASSERT_TRUE(strong.shakeLevel() <= 1.0f);
  TEST_ASSERT_TRUE(weak.shakeLevel() >= 0.0f);
}

// Фильтры обязаны работать по измеренному dt (§5): кадр с перезаливкой
// экрана растягивает тик с 10 до 22 мс, и пороги не должны от этого плыть.
void test_result_is_dt_independent() {
  ShakeDetector fast; fast.reset();
  ShakeDetector slow; slow.reset();
  EventCounts c1, c2;
  feedRest(fast, c1, 300, 10);  feedConstant(fast, c1, 0.9f, 600, 10);
  feedRest(slow, c2, 300, 22);  feedConstant(slow, c2, 0.9f, 600, 22);
  TEST_ASSERT_EQUAL_INT(1, c1.started);
  TEST_ASSERT_EQUAL_INT(1, c2.started);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, c1.firstIntensity, c2.firstIntensity);
}

void test_motion_seen_flag() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedRest(d, c, 500);
  TEST_ASSERT_FALSE(d.motionSeen());
  feedConstant(d, c, 0.5f, 100);
  TEST_ASSERT_TRUE(d.motionSeen());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_rest_yields_no_events);
  RUN_TEST(test_shake_starts_once_and_holds);
  RUN_TEST(test_shake_ends_after_quiet);
  RUN_TEST(test_weak_motion_below_threshold_is_ignored);
  RUN_TEST(test_refractory_blocks_immediate_retrigger);
  RUN_TEST(test_start_intensity_monotonic_in_amplitude);
  RUN_TEST(test_shake_level_grows_with_amplitude);
  RUN_TEST(test_result_is_dt_independent);
  RUN_TEST(test_motion_seen_flag);
  return UNITY_END();
}
```

- [ ] **Step 3: Подключить `ShakeDetector.cpp` к нативной сборке**

В `platformio.ini`, окружение `[env:native]`, заменить строку:

```ini
build_src_filter = -<*>
```

на

```ini
build_src_filter = -<*> +<ShakeDetector.cpp>
```

- [ ] **Step 4: Прогнать тесты и убедиться, что они падают**

```bash
pio test -e native -f test_shake
```

Ожидается: ошибка компиляции `ShakeDetector.h: No such file or directory`.

- [ ] **Step 5: Написать заголовок `src/ShakeDetector.h`**

```cpp
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
```

- [ ] **Step 6: Написать реализацию `src/ShakeDetector.cpp`**

```cpp
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
```

- [ ] **Step 7: Прогнать тесты и убедиться, что они проходят**

```bash
pio test -e native -f test_shake
```

Ожидается: 9 тестов PASSED. Если `test_shake_starts_once_and_holds` даёт больше 3 событий — проверить, что пороги применяются к `_excess`, а не к `_rawExcess`.

- [ ] **Step 8: Прогнать весь нативный набор и собрать прошивку**

```bash
pio test -e native && pio run -e m5stack-stickc-plus2
```

Ожидается: все тесты PASSED, сборка SUCCESS.

- [ ] **Step 9: Коммит**

```bash
git add platformio.ini src/ShakeDetector.h src/ShakeDetector.cpp test/test_shake
git commit -m "feat: shake detector core with hysteresis state machine and dt-driven filters"
```

---

### Task 4: `ShakeDetector` — фильтр падений, наклон, скорость движения

**Files:**
- Modify: `src/ShakeDetector.h`, `src/ShakeDetector.cpp`
- Modify: `test/test_shake/signals.h`, `test/test_shake/test_shake.cpp`
- Create: `test/test_shake/trace.h`

**Interfaces:**
- Consumes: `ShakeDetector` из Task 3.
- Produces: добавленные к `ShakeDetector` методы
  ```cpp
  float tiltAngle()  const;   // 0..180°, между осью kTiltAxis и гравитацией
  float motionRate() const;   // °/с, модуль вектора гироскопа
  bool  suppressed() const;   // активно окно подавления событий
  ```
  и функция загрузки трасс `bool loadTrace(const char* path, TraceResult& out)` в `test/test_shake/trace.h`.

- [ ] **Step 1: Дописать генераторы падения и шлепка**

Добавить в конец `test/test_shake/signals.h` (перед закрывающей частью файла нет — файл состоит только из функций, дописываем в конец):

```cpp
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
```

- [ ] **Step 2: Написать загрузчик трасс**

Создать `test/test_shake/trace.h`:

```cpp
#pragma once
#include <stdio.h>
#include <stdlib.h>

#include "ShakeDetector.h"

struct TraceResult {
  int  rows    = 0;
  int  started = 0;
  int  ended   = 0;
  bool loaded  = false;
};

// Формат CSV из MARACA_TRACE (Task 7):
// t_ms,ax,ay,az,gx,gy,gz,excess,state,ev
// Первая строка — заголовок. Трассы хранятся в test/traces/.
inline TraceResult replayTrace(const char* path) {
  TraceResult r;
  FILE* f = fopen(path, "r");
  if (!f) return r;
  r.loaded = true;

  char line[256];
  if (!fgets(line, sizeof(line), f)) { fclose(f); return r; }

  ShakeDetector d;
  d.reset();
  uint32_t prevT = 0;
  bool first = true;
  while (fgets(line, sizeof(line), f)) {
    unsigned long t = 0;
    float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    if (sscanf(line, "%lu,%f,%f,%f,%f,%f,%f", &t, &ax, &ay, &az, &gx, &gy, &gz) != 7) {
      continue;
    }
    uint32_t dt = first ? 10u : (uint32_t)(t - prevT);
    if (dt == 0) dt = 1;
    if (dt > 100) dt = 100;
    first = false;
    prevT = (uint32_t)t;

    d.update(ax, ay, az, gx, gy, gz, dt);
    ++r.rows;
    if (d.shakeStarted()) ++r.started;
    if (d.shakeEnded())   ++r.ended;
  }
  fclose(f);
  return r;
}
```

- [ ] **Step 3: Написать падающие тесты падений, наклона и трасс**

Добавить в `test/test_shake/test_shake.cpp` — директиву `#include "trace.h"` после `#include "signals.h"`, перечисленные ниже тесты перед `main`, и соответствующие `RUN_TEST` в `main`:

```cpp
void test_drop_from_table_yields_no_events() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedDrop(d, c);
  TEST_ASSERT_EQUAL_INT(0, c.started);
}

void test_slap_on_table_yields_no_events() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedSlap(d, c);
  TEST_ASSERT_EQUAL_INT(0, c.started);
}

void test_suppression_expires_and_shaking_works_again() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedSlap(d, c);
  TEST_ASSERT_FALSE(d.suppressed());
  feedShake(d, c, 0.9f, 8.0f, 800);
  TEST_ASSERT_TRUE(c.started >= 1);
}

void test_tilt_angle_full_range() {
  const float probes[4] = {0.0f, 45.0f, 90.0f, 180.0f};
  for (int i = 0; i < 4; ++i) {
    ShakeDetector d; d.reset();
    EventCounts c;
    feedTilt(d, c, probes[i]);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, probes[i], d.tiltAngle());
  }
}

void test_tilt_is_stable_under_shaking() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedTilt(d, c, 90.0f);
  const float before = d.tiltAngle();
  feedShake(d, c, 0.8f, 9.0f, 1000);
  // Тряска идёт по z, но фильтр гравитации 0,3 с не должен уводить угол далеко.
  TEST_ASSERT_FLOAT_WITHIN(25.0f, before, d.tiltAngle());
}

void test_motion_rate_is_gyro_magnitude() {
  ShakeDetector d; d.reset();
  d.update(0.0f, 0.0f, 1.0f, 30.0f, 40.0f, 0.0f, 10);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, d.motionRate());
  d.update(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 10);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, d.motionRate());
}

// Трассы с железа (Task 7). Пока файлов нет — тест сообщает ignore, а не падает.
void test_recorded_traces() {
  struct Case { const char* path; int maxStarted; int minStarted; };
  const Case cases[4] = {
    {"test/traces/rest_sofa.csv",   0, 0},
    {"test/traces/drop_table.csv",  0, 0},
    {"test/traces/slap_table.csv",  0, 0},
    {"test/traces/baby_shake.csv", 99, 1},
  };
  int checked = 0;
  for (int i = 0; i < 4; ++i) {
    TraceResult r = replayTrace(cases[i].path);
    if (!r.loaded) continue;
    ++checked;
    TEST_ASSERT_TRUE_MESSAGE(r.rows > 0, cases[i].path);
    TEST_ASSERT_TRUE_MESSAGE(r.started >= cases[i].minStarted, cases[i].path);
    TEST_ASSERT_TRUE_MESSAGE(r.started <= cases[i].maxStarted, cases[i].path);
  }
  if (checked == 0) {
    TEST_IGNORE_MESSAGE("no traces in test/traces/ yet, see Task 7");
  }
}
```

- [ ] **Step 4: Прогнать тесты и убедиться, что они падают**

```bash
pio test -e native -f test_shake
```

Ожидается: ошибка компиляции — `tiltAngle`, `motionRate`, `suppressed` не являются членами `ShakeDetector`.

- [ ] **Step 5: Расширить заголовок**

В `src/ShakeDetector.h` добавить в публичную секцию (после `stateCode()`):

```cpp
  float tiltAngle()  const;        // 0..180°
  float motionRate() const { return _motionRate; }
  bool  suppressed() const { return _suppressMs > 0 || _inFreefall; }
```

и в приватную секцию (после `_primed`):

```cpp
  float    _motionRate       = 0.0f;
  float    _tiltAngle        = 0.0f;
  uint32_t _freefallMs       = 0;    // накопленная длительность текущего падения
  uint32_t _sinceFreefallMs  = 0xFFFFFFFFu;  // с момента подтверждённого падения
  uint32_t _suppressMs       = 0;    // остаток окна подавления
  bool     _inFreefall       = false;
```

- [ ] **Step 6: Реализовать фильтр падений, наклон и скорость**

В `src/ShakeDetector.cpp` внести три правки.

Правка 1 — вместо строки `(void)gx; (void)gy; (void)gz;  // используются в Task 4` поставить вычисление скорости:

```cpp
  _motionRate = sqrtf(gx * gx + gy * gy + gz * gz);
```

Правка 2 — сразу после блока фильтров (`lowpass(...)` / инициализации `_primed`) и перед строкой `_motionSeen = ...` вставить:

```cpp
  // Свободное падение даёт |a| ~ 0, то есть excess ~ 1,0 g — выше kShakeOn.
  // Поэтому события гасятся уже по мгновенному признаку, не дожидаясь
  // подтверждения длительностью kFreefallMs.
  _inFreefall = mag < kFreefallG;
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
```

Правка 3 — добавить в конец файла:

```cpp
float ShakeDetector::tiltAngle() const {
  return _tiltAngle;
}
```

- [ ] **Step 7: Прогнать тесты и убедиться, что они проходят**

```bash
pio test -e native -f test_shake
```

Ожидается: все тесты PASSED, `test_recorded_traces` — IGNORE с сообщением про Task 7.

Если `test_tilt_angle_full_range` падает на 0° — проверить, что `feedTilt(0)` даёт `ax = 1`, а `kTiltAxis == 0`: угол 0° означает «гравитация вдоль длинной оси».

- [ ] **Step 8: Прогнать весь набор и собрать прошивку**

```bash
pio test -e native && pio run -e m5stack-stickc-plus2
```

- [ ] **Step 9: Коммит**

```bash
git add src/ShakeDetector.h src/ShakeDetector.cpp test/test_shake
git commit -m "feat: freefall/impact suppression, tilt angle and gyro motion rate"
```

---

### Task 5: `Buzzer` — пентатоника и секвенсор нот

**Files:**
- Create: `src/Buzzer.h`, `src/Buzzer.cpp`
- Create: `test/test_music/test_music.cpp`
- Modify: `platformio.ini` (`build_src_filter` окружения `native`)

**Interfaces:**
- Consumes: константы `kBaseHz`, `kScale`, `kStepMax`, `kNoteCapacity`, `enum class VolumeLevel` из `src/Config.h`.
- Produces:
  ```cpp
  struct Note { int16_t step; uint16_t durMs; uint16_t gapMs; };

  class Buzzer {
   public:
    void   begin();
    void   update(uint32_t now);
    void   play(const Note* notes, size_t n);
    void   push(const Note& n);
    void   stop();
    bool   isPlaying() const;
    void   setTempoScale(float s);
    size_t pending() const;
    static float pitchOf(int step);
   protected:
    virtual void emitTone(float hz, uint16_t durMs);   // шов для тестов
    virtual void emitStop();
  };
  ```
  (`glide()`, `setVolumeLevel()`, `volume()`, `gliding()` добавляются в Task 6.)

- [ ] **Step 1: Написать падающие тесты пентатоники и секвенсора**

Создать `test/test_music/test_music.cpp`:

```cpp
#include <unity.h>

#include <math.h>
#include <vector>

#include "Buzzer.h"
#include "Config.h"

struct Emitted {
  float    hz;
  uint16_t durMs;
  uint32_t atMs;
};

// Шов: вместо M5.Speaker пишем в журнал.
class TestBuzzer : public Buzzer {
 public:
  std::vector<Emitted> tones;
  int stops = 0;
  uint32_t now = 0;

  void run(uint32_t ms, uint32_t stepMs = 10) {
    for (uint32_t t = 0; t < ms; t += stepMs) {
      now += stepMs;
      update(now);
    }
  }

 protected:
  void emitTone(float hz, uint16_t durMs) override {
    tones.push_back({hz, durMs, now});
  }
  void emitStop() override { ++stops; }
};

void test_pitch_is_monotonic_and_in_band() {
  float prev = 0.0f;
  for (int s = 0; s <= kStepMax; ++s) {
    const float hz = Buzzer::pitchOf(s);
    TEST_ASSERT_TRUE(hz > prev);
    TEST_ASSERT_TRUE(hz >= 698.0f);   // kBaseHz = F5 = 698,46 — пол диапазона
    TEST_ASSERT_TRUE(hz <= 3000.0f);
    prev = hz;
  }
}

void test_pitch_octave_is_exact_doubling() {
  TEST_ASSERT_FLOAT_WITHIN(0.5f, kBaseHz, Buzzer::pitchOf(0));
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 2.0f * kBaseHz, Buzzer::pitchOf(5));
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 2349.0f, Buzzer::pitchOf(9));
}

void test_pitch_clamps_out_of_range_steps() {
  TEST_ASSERT_EQUAL_FLOAT(Buzzer::pitchOf(0), Buzzer::pitchOf(-3));
  TEST_ASSERT_EQUAL_FLOAT(Buzzer::pitchOf(kStepMax), Buzzer::pitchOf(kStepMax + 7));
}

void test_sequence_plays_in_order_with_gaps() {
  TestBuzzer b;
  b.begin();
  const Note seq[3] = {{0, 100, 50}, {2, 100, 50}, {4, 100, 50}};
  b.play(seq, 3);
  b.run(600);

  TEST_ASSERT_EQUAL_INT(3, (int)b.tones.size());
  TEST_ASSERT_FLOAT_WITHIN(1.0f, Buzzer::pitchOf(0), b.tones[0].hz);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, Buzzer::pitchOf(2), b.tones[1].hz);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, Buzzer::pitchOf(4), b.tones[2].hz);
  TEST_ASSERT_EQUAL_UINT16(100, b.tones[0].durMs);
  // вторая нота не раньше, чем через dur + gap после первой
  TEST_ASSERT_TRUE(b.tones[1].atMs - b.tones[0].atMs >= 150);
  TEST_ASSERT_TRUE(b.tones[1].atMs - b.tones[0].atMs <= 170);
}

void test_negative_step_is_a_rest() {
  TestBuzzer b;
  b.begin();
  const Note seq[3] = {{0, 100, 0}, {-1, 200, 0}, {4, 100, 0}};
  b.play(seq, 3);
  b.run(600);

  TEST_ASSERT_EQUAL_INT(2, (int)b.tones.size());
  TEST_ASSERT_FLOAT_WITHIN(1.0f, Buzzer::pitchOf(4), b.tones[1].hz);
  // пауза съела свои 200 мс
  TEST_ASSERT_TRUE(b.tones[1].atMs - b.tones[0].atMs >= 300);
}

void test_tempo_scale_stretches_notes_and_gaps() {
  TestBuzzer b;
  b.begin();
  b.setTempoScale(2.0f);
  const Note seq[2] = {{0, 100, 50}, {2, 100, 50}};
  b.play(seq, 2);
  b.run(800);

  TEST_ASSERT_EQUAL_INT(2, (int)b.tones.size());
  TEST_ASSERT_EQUAL_UINT16(200, b.tones[0].durMs);
  TEST_ASSERT_TRUE(b.tones[1].atMs - b.tones[0].atMs >= 300);
}

void test_push_appends_to_tail() {
  TestBuzzer b;
  b.begin();
  const Note first[1] = {{0, 60, 20}};
  b.play(first, 1);
  b.push({7, 60, 20});
  b.run(400);
  TEST_ASSERT_EQUAL_INT(2, (int)b.tones.size());
  TEST_ASSERT_FLOAT_WITHIN(1.0f, Buzzer::pitchOf(7), b.tones[1].hz);
}

// §12: переполнение буфера отбрасывает новую ноту, а не блокирует.
void test_overflow_drops_new_notes() {
  TestBuzzer b;
  b.begin();
  for (int i = 0; i < 40; ++i) b.push({0, 10, 0});
  TEST_ASSERT_EQUAL_UINT32((uint32_t)kNoteCapacity, (uint32_t)b.pending());
  b.run(1000);
  TEST_ASSERT_EQUAL_INT((int)kNoteCapacity, (int)b.tones.size());
}

void test_is_playing_tracks_queue_and_tail() {
  TestBuzzer b;
  b.begin();
  const Note seq[1] = {{0, 100, 50}};
  b.play(seq, 1);
  b.run(20);
  TEST_ASSERT_TRUE(b.isPlaying());
  b.run(400);
  TEST_ASSERT_FALSE(b.isPlaying());
}

void test_play_replaces_queue() {
  TestBuzzer b;
  b.begin();
  const Note a[3] = {{0, 100, 50}, {0, 100, 50}, {0, 100, 50}};
  b.play(a, 3);
  b.run(20);
  const Note c[1] = {{9, 50, 0}};
  b.play(c, 1);
  b.run(300);
  // первая нота первой последовательности успела прозвучать, остальные — нет
  TEST_ASSERT_EQUAL_INT(2, (int)b.tones.size());
  TEST_ASSERT_FLOAT_WITHIN(1.0f, Buzzer::pitchOf(9), b.tones[1].hz);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_pitch_is_monotonic_and_in_band);
  RUN_TEST(test_pitch_octave_is_exact_doubling);
  RUN_TEST(test_pitch_clamps_out_of_range_steps);
  RUN_TEST(test_sequence_plays_in_order_with_gaps);
  RUN_TEST(test_negative_step_is_a_rest);
  RUN_TEST(test_tempo_scale_stretches_notes_and_gaps);
  RUN_TEST(test_push_appends_to_tail);
  RUN_TEST(test_overflow_drops_new_notes);
  RUN_TEST(test_is_playing_tracks_queue_and_tail);
  RUN_TEST(test_play_replaces_queue);
  return UNITY_END();
}
```

- [ ] **Step 2: Подключить `Buzzer.cpp` к нативной сборке**

В `platformio.ini`, окружение `[env:native]`:

```ini
build_src_filter = -<*> +<ShakeDetector.cpp> +<Buzzer.cpp>
```

- [ ] **Step 3: Прогнать тесты и убедиться, что они падают**

```bash
pio test -e native -f test_music
```

Ожидается: ошибка компиляции `Buzzer.h: No such file or directory`.

- [ ] **Step 4: Написать `src/Buzzer.h`**

```cpp
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "Config.h"

struct Note {
  int16_t  step;    // ступень пентатоники; отрицательная = пауза
  uint16_t durMs;
  uint16_t gapMs;
};

// Секвенсор нот поверх M5.Speaker: темп с паузами, пентатоническое
// квантование, потолок громкости. Единственный владелец M5.Speaker.
class Buzzer {
 public:
  virtual ~Buzzer() = default;

  void   begin();
  void   update(uint32_t now);
  void   play(const Note* notes, size_t n);   // очистить очередь и проиграть
  void   push(const Note& n);                 // досыпать в хвост
  void   stop();
  bool   isPlaying() const;
  void   setTempoScale(float s);              // растягивает durMs и gapMs
  size_t pending() const { return _count; }

  static float pitchOf(int step);

 protected:
  // Швы для нативных тестов. На железе бьют в M5.Speaker.
  virtual void emitTone(float hz, uint16_t durMs);
  virtual void emitStop();

  Note     _ring[kNoteCapacity];
  size_t   _head   = 0;
  size_t   _count  = 0;
  uint32_t _nextMs = 0;        // когда можно выдать следующую ноту
  uint32_t _lastNow = 0;
  float    _tempo  = 1.0f;
};
```

- [ ] **Step 5: Написать `src/Buzzer.cpp`**

```cpp
#include "Buzzer.h"

#include <math.h>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

namespace {
inline uint16_t scaled(uint16_t ms, float tempo) {
  const float v = (float)ms * tempo;
  return (uint16_t)(v < 0.0f ? 0.0f : (v > 65535.0f ? 65535.0f : v));
}
}  // namespace

void Buzzer::begin() {
  _head = 0;
  _count = 0;
  _nextMs = 0;
  _lastNow = 0;
  _tempo = 1.0f;
}

float Buzzer::pitchOf(int step) {
  if (step < 0) step = 0;
  if (step > kStepMax) step = kStepMax;
  const int octave = step / 5;
  const int index  = step % 5;
  const float semitone = (float)kScale[index] + 12.0f * (float)octave;
  return kBaseHz * powf(2.0f, semitone / 12.0f);
}

void Buzzer::play(const Note* notes, size_t n) {
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;        // первая нота звучит сразу
  for (size_t i = 0; i < n; ++i) push(notes[i]);
}

void Buzzer::push(const Note& n) {
  if (_count >= kNoteCapacity) return;   // §12: новая нота отбрасывается
  _ring[(_head + _count) % kNoteCapacity] = n;
  ++_count;
}

void Buzzer::stop() {
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;
  emitStop();
}

bool Buzzer::isPlaying() const {
  return _count > 0 || _lastNow < _nextMs;
}

void Buzzer::setTempoScale(float s) {
  if (s < 0.25f) s = 0.25f;
  if (s > 4.0f)  s = 4.0f;
  _tempo = s;
}

void Buzzer::update(uint32_t now) {
  _lastNow = now;
  if (_count == 0) return;
  if (now < _nextMs) return;

  const Note n = _ring[_head];
  _head = (_head + 1) % kNoteCapacity;
  --_count;

  const uint16_t dur = scaled(n.durMs, _tempo);
  const uint16_t gap = scaled(n.gapMs, _tempo);
  if (n.step >= 0) emitTone(pitchOf(n.step), dur);
  _nextMs = now + dur + gap;
}

void Buzzer::emitTone(float hz, uint16_t durMs) {
#if defined(ARDUINO)
  // stop_current_sound = false: обрыв предыдущей ноты на полуфазе даёт щелчок.
  M5.Speaker.tone(hz, durMs, -1, false);
#else
  (void)hz; (void)durMs;
#endif
}

void Buzzer::emitStop() {
#if defined(ARDUINO)
  M5.Speaker.stop();
#endif
}
```

- [ ] **Step 6: Прогнать тесты и убедиться, что они проходят**

```bash
pio test -e native -f test_music
```

Ожидается: 10 тестов PASSED.

- [ ] **Step 7: Прогнать весь набор и собрать прошивку**

```bash
pio test -e native && pio run -e m5stack-stickc-plus2
```

- [ ] **Step 8: Коммит**

```bash
git add platformio.ini src/Buzzer.h src/Buzzer.cpp test/test_music
git commit -m "feat: pentatonic pitch mapping and non-blocking note sequencer"
```

---

### Task 6: `Buzzer` — потолок громкости, glide, привязка к `M5.Speaker`

**Files:**
- Modify: `src/Buzzer.h`, `src/Buzzer.cpp`
- Modify: `test/test_music/test_music.cpp`

**Interfaces:**
- Consumes: `Buzzer` из Task 5, `kMaxMasterVolume`, `kVolumeQuietPct`, `VolumeLevel`.
- Produces: добавленные к `Buzzer` методы
  ```cpp
  void    glide(float hz);              // непрерывный тон, забирает динамик
  void    setVolumeLevel(VolumeLevel v);
  uint8_t volume() const;               // фактическая громкость 0..kMaxMasterVolume
  bool    gliding() const;
  ```
  и швы `virtual void emitGlide(float hz)`, `virtual void applyVolume(uint8_t v)`.

- [ ] **Step 1: Дописать падающие тесты громкости и глиссандо**

В `test/test_music/test_music.cpp`: добавить в `TestBuzzer` поля и переопределения, затем тесты и `RUN_TEST`-строки.

В класс `TestBuzzer`, в публичную секцию:

```cpp
  std::vector<float> glides;
  std::vector<uint8_t> volumes;
```

в защищённую секцию:

```cpp
  void emitGlide(float hz) override { glides.push_back(hz); }
  void applyVolume(uint8_t v) override { volumes.push_back(v); }
```

Новые тесты (перед `main`):

```cpp
void test_volume_levels_respect_ceiling() {
  TestBuzzer b;
  b.begin();
  b.setVolumeLevel(VolumeLevel::Normal);
  TEST_ASSERT_EQUAL_UINT8(kMaxMasterVolume, b.volume());
  b.setVolumeLevel(VolumeLevel::Quiet);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)(kMaxMasterVolume * kVolumeQuietPct / 100), b.volume());
  b.setVolumeLevel(VolumeLevel::Mute);
  TEST_ASSERT_EQUAL_UINT8(0, b.volume());
  for (size_t i = 0; i < b.volumes.size(); ++i) {
    TEST_ASSERT_TRUE(b.volumes[i] <= kMaxMasterVolume);
  }
}

// §8: Mute — это громкость 0, а не остановка секвенсора: визуализация
// остаётся синхронной, а Power по-прежнему видит isPlaying().
void test_mute_keeps_sequence_running() {
  TestBuzzer b;
  b.begin();
  b.setVolumeLevel(VolumeLevel::Mute);
  const Note seq[2] = {{0, 80, 20}, {4, 80, 20}};
  b.play(seq, 2);
  b.run(60);
  TEST_ASSERT_TRUE(b.isPlaying());
  b.run(400);
  TEST_ASSERT_EQUAL_INT(2, (int)b.tones.size());
  TEST_ASSERT_EQUAL_UINT8(0, b.volume());
}

void test_glide_takes_over_and_cancels_queue() {
  TestBuzzer b;
  b.begin();
  const Note seq[3] = {{0, 100, 50}, {2, 100, 50}, {4, 100, 50}};
  b.play(seq, 3);
  b.run(20);
  b.glide(1200.0f);
  TEST_ASSERT_TRUE(b.gliding());
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)b.pending());
  TEST_ASSERT_EQUAL_INT(1, (int)b.glides.size());
  b.run(500);
  // пока идёт глиссандо, ноты не выдаются
  TEST_ASSERT_EQUAL_INT(1, (int)b.tones.size());
  TEST_ASSERT_TRUE(b.isPlaying());
}

void test_glide_reissues_only_on_pitch_change() {
  TestBuzzer b;
  b.begin();
  b.glide(1000.0f);
  b.glide(1000.0f);
  b.glide(1000.4f);
  TEST_ASSERT_EQUAL_INT(1, (int)b.glides.size());
  b.glide(1400.0f);
  TEST_ASSERT_EQUAL_INT(2, (int)b.glides.size());
}

void test_push_cancels_glide() {
  TestBuzzer b;
  b.begin();
  b.glide(1000.0f);
  b.push({0, 60, 20});
  TEST_ASSERT_FALSE(b.gliding());
  TEST_ASSERT_EQUAL_INT(1, b.stops);
  b.run(200);
  TEST_ASSERT_EQUAL_INT(1, (int)b.tones.size());
}

void test_stop_ends_glide_and_playing() {
  TestBuzzer b;
  b.begin();
  b.glide(1000.0f);
  b.stop();
  TEST_ASSERT_FALSE(b.gliding());
  TEST_ASSERT_FALSE(b.isPlaying());
}
```

и в `main`:

```cpp
  RUN_TEST(test_volume_levels_respect_ceiling);
  RUN_TEST(test_mute_keeps_sequence_running);
  RUN_TEST(test_glide_takes_over_and_cancels_queue);
  RUN_TEST(test_glide_reissues_only_on_pitch_change);
  RUN_TEST(test_push_cancels_glide);
  RUN_TEST(test_stop_ends_glide_and_playing);
```

- [ ] **Step 2: Прогнать тесты и убедиться, что они падают**

```bash
pio test -e native -f test_music
```

Ожидается: ошибка компиляции — `glide`, `setVolumeLevel`, `volume`, `gliding` не являются членами `Buzzer`.

- [ ] **Step 3: Расширить `src/Buzzer.h`**

В публичную секцию (после `setTempoScale`):

```cpp
  void    glide(float hz);              // непрерывный тон до stop()
  void    setVolumeLevel(VolumeLevel v);
  uint8_t volume()  const { return _volume; }
  bool    gliding() const { return _gliding; }
```

В защищённую секцию (после `emitStop`):

```cpp
  virtual void emitGlide(float hz);
  virtual void applyVolume(uint8_t v);

  uint8_t _volume  = 0;
  bool    _gliding = false;
  float   _glideHz = 0.0f;
```

- [ ] **Step 4: Реализовать в `src/Buzzer.cpp`**

Правка 1 — в `begin()` после `_tempo = 1.0f;` добавить:

```cpp
  _gliding = false;
  _glideHz = 0.0f;
  setVolumeLevel(VolumeLevel::Quiet);   // §12: громкость по умолчанию
```

Правка 2 — в `play()` первой строкой тела добавить снятие глиссандо:

```cpp
  if (_gliding) { _gliding = false; emitStop(); }
```

Правка 3 — в `push()` перед проверкой переполнения добавить ту же строку:

```cpp
  if (_gliding) { _gliding = false; emitStop(); }
```

Правка 4 — в `stop()` перед `emitStop()` добавить:

```cpp
  _gliding = false;
  _glideHz = 0.0f;
```

Правка 5 — `isPlaying()` учитывает глиссандо:

```cpp
bool Buzzer::isPlaying() const {
  return _gliding || _count > 0 || _lastNow < _nextMs;
}
```

Правка 6 — в начало `update()` после `_lastNow = now;` добавить:

```cpp
  if (_gliding) return;   // динамик в исключительном владении глиссандо
```

Правка 7 — добавить в конец файла:

```cpp
void Buzzer::glide(float hz) {
  _head = 0;
  _count = 0;
  _nextMs = _lastNow;
  // Повторная выдача тона на той же частоте дала бы щелчок на ровном месте.
  if (!_gliding || fabsf(hz - _glideHz) > 1.0f) {
    _glideHz = hz;
    emitGlide(hz);
  }
  _gliding = true;
}

void Buzzer::setVolumeLevel(VolumeLevel v) {
  switch (v) {
    case VolumeLevel::Quiet:
      _volume = (uint8_t)((uint16_t)kMaxMasterVolume * kVolumeQuietPct / 100);
      break;
    case VolumeLevel::Normal:
      _volume = kMaxMasterVolume;
      break;
    case VolumeLevel::Mute:
    default:
      _volume = 0;
      break;
  }
  applyVolume(_volume);
}

void Buzzer::emitGlide(float hz) {
#if defined(ARDUINO)
  // Бесконечный тон на выделенном канале; смена частоты перебивает прежний.
  M5.Speaker.tone(hz, UINT32_MAX, 0, true);
#else
  (void)hz;
#endif
}

void Buzzer::applyVolume(uint8_t v) {
#if defined(ARDUINO)
  // Единственное место в проекте, где вызывается setVolume (§8).
  M5.Speaker.setVolume(v);
#else
  (void)v;
#endif
}
```

- [ ] **Step 5: Прогнать тесты и убедиться, что они проходят**

```bash
pio test -e native -f test_music
```

Ожидается: 16 тестов PASSED.

Если `test_volume_levels_respect_ceiling` видит лишний элемент в `volumes` — это вызов из `begin()`; тест проверяет только потолок всех записей, так что он должен проходить. Если падает `test_push_cancels_glide` по числу `stops` — убедиться, что `begin()` не вызывает `emitStop()`.

- [ ] **Step 6: Проверить отсутствие других вызовов `setVolume` и `tone`**

```bash
grep -rn "M5.Speaker" src/ | grep -v "^src/Buzzer.cpp"
```

Ожидается: пустой вывод.

- [ ] **Step 7: Прогнать весь набор и собрать прошивку**

```bash
pio test -e native && pio run -e m5stack-stickc-plus2
```

- [ ] **Step 8: Коммит**

```bash
git add src/Buzzer.h src/Buzzer.cpp test/test_music
git commit -m "feat: volume ceiling, mute as zero volume, exclusive glide mode"
```

---

### Task 7: `Accel` — слой над `M5.Imu`, калибровка оси наклона, сбор трасс

**Files:**
- Create: `src/Accel.h`, `src/Accel.cpp`
- Create: `src/trace_main.cpp`
- Create: `test/traces/.gitkeep`
- Modify: `platformio.ini` (окружение `trace`)
- Modify: `src/Config.h` (возможная правка `kTiltAxis` по результату калибровки)
- Modify: `docs/hw-notes.md`

**Interfaces:**
- Consumes: `ShakeDetector` (Tasks 3–4).
- Produces:
  ```cpp
  class Accel {
   public:
    void     begin();
    void     update(uint32_t nowMs);
    bool     isEnabled() const;
    bool     shakeStarted() const;
    bool     shakeEnded() const;
    float    startIntensity() const;   // 0..1
    bool     isShaking() const;
    float    shakeLevel() const;       // 0..1
    float    tiltAngle() const;        // 0..180°
    float    motionRate() const;       // °/с
    uint32_t msSinceMotion(uint32_t now) const;
    float    excess() const;
    uint8_t  stateCode() const;
  };
  ```
  Окружение сборки `trace` и CSV-формат `t_ms,ax,ay,az,gx,gy,gz,excess,state,ev`, который читает `replayTrace()` из Task 4.

- [ ] **Step 1: Написать `src/Accel.h`**

```cpp
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
```

- [ ] **Step 2: Написать `src/Accel.cpp`**

```cpp
#include "Accel.h"

#include <M5Unified.h>

#include "Config.h"

void Accel::begin() {
  _enabled = M5.Imu.isEnabled();
  _det.reset();
  _primed = false;
  _lastUpdateMs = 0;
  _lastMotionMs = 0;

#if MARACA_TRACE
  Serial.println("t_ms,ax,ay,az,gx,gy,gz,excess,state,ev");
#endif
}

void Accel::update(uint32_t nowMs) {
  if (!_enabled) return;

  uint32_t dtMs = kTickMs;
  if (_primed) {
    dtMs = nowMs - _lastUpdateMs;
    if (dtMs == 0) dtMs = 1;
    if (dtMs > 100) dtMs = 100;   // после сплэша или долгой отрисовки
  } else {
    _primed = true;
    _lastMotionMs = nowMs;
  }
  _lastUpdateMs = nowMs;

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  M5.Imu.getAccel(&ax, &ay, &az);   // g
  M5.Imu.getGyro(&gx, &gy, &gz);    // °/с

  _det.update(ax, ay, az, gx, gy, gz, dtMs);

  if (_det.motionSeen()) _lastMotionMs = nowMs;

#if MARACA_TRACE
  const int ev = _det.shakeStarted() ? 1 : (_det.shakeEnded() ? 2 : 0);
  Serial.printf("%lu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.4f,%u,%d\n",
                (unsigned long)nowMs, ax, ay, az, gx, gy, gz,
                _det.excess(), (unsigned)_det.stateCode(), ev);
#endif
}
```

- [ ] **Step 3: Написать прошивку сбора трасс**

Создать `src/trace_main.cpp`:

```cpp
// Прошивка для снятия трасс и калибровки kTiltAxis.
// Собирается только в окружении env:trace (см. platformio.ini).
#include <M5Unified.h>

#include "Accel.h"
#include "Config.h"

static Accel accel;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(0);
  M5.Display.setBrightness(kMaxBrightness);
  M5.Display.fillScreen(0x0000);
  M5.Display.setTextColor(0xFFFF, 0x0000);
  accel.begin();
  if (!accel.isEnabled()) {
    M5.Display.setCursor(4, 4);
    M5.Display.print("NO IMU");
  }
}

void loop() {
  const uint32_t now = millis();
  M5.update();
  accel.update(now);

  // Калибровочная подсказка на экране: раз в 250 мс, вне CSV.
  static uint32_t lastShow = 0;
  if (now - lastShow >= 250) {
    lastShow = now;
    M5.Display.fillRect(0, 0, kScreenWidth, 60, 0x0000);
    M5.Display.setCursor(2, 2);
    M5.Display.printf("tilt %5.1f\nrate %5.0f\nlvl  %4.2f",
                      accel.tiltAngle(), accel.motionRate(), accel.shakeLevel());
  }
  M5.delay(kTickMs);
}
```

- [ ] **Step 4: Добавить окружение `trace` в `platformio.ini`**

Дописать в конец файла:

```ini
; Прошивка для снятия CSV-трасс (§11). Штатный main.cpp исключён.
[env:trace]
extends = env:m5stack-stickc-plus2
build_src_filter = +<*> -<main.cpp>
build_flags =
	${env:m5stack-stickc-plus2.build_flags}
	-D MARACA_TRACE=1
```

- [ ] **Step 5: Собрать оба окружения**

```bash
pio run -e m5stack-stickc-plus2 && pio run -e trace
```

Ожидается: оба SUCCESS. Прошивка `trace` должна собираться без `main.cpp` — если линковщик жалуется на дублирование `setup`/`loop`, проверить `build_src_filter`.

- [ ] **Step 6: Прошить трассировочную прошивку и откалибровать `kTiltAxis` [железо]**

```bash
pio run -e trace -t upload
```

Затем, не открывая монитор одновременно с прошивкой:

```bash
pio device monitor -e trace
```

Калибровка: держать устройство экраном к себе, затем
1. поставить вертикально, разъёмом USB вниз → записать показанный `tilt`;
2. положить плашмя экраном вверх → записать `tilt`;
3. перевернуть разъёмом вверх → записать `tilt`.

Требование: между положениями 1 и 3 угол должен пройти почти полный размах (разница > 150°), а положение 2 дать примерно середину. Если размах мал — поменять `kTiltAxis` в `src/Config.h` на 1, затем на 2, пересобрать и повторить. Зафиксировать выбранное значение.

- [ ] **Step 7: Снять четыре трассы [железо]**

Каждую трассу снимать отдельно: открыть монитор, выполнить движение, закрыть монитор, сохранить вывод в файл. Строку заголовка `t_ms,ax,...` оставить первой, строки с подсказками экрана в CSV не попадают.

```bash
pio device monitor -e trace --quiet | tee test/traces/rest_sofa.csv
```

Трассы и движения:
1. `test/traces/rest_sofa.csv` — устройство лежит неподвижно на диване, 20 с.
2. `test/traces/baby_shake.csv` — ребёнок (или имитация: рука, 5–15 Гц) трясёт, 15 с.
3. `test/traces/drop_table.csv` — падение с высоты стола на мягкое (подушка/ковёр), 3 повтора с паузами.
4. `test/traces/slap_table.csv` — шлепок устройством об стол, 3 повтора с паузами.

После сохранения вычистить из файлов строки, не начинающиеся с цифры:

```bash
for f in test/traces/*.csv; do head -1 "$f" > "$f.tmp" && grep -E '^[0-9]+,' "$f" >> "$f.tmp" && mv "$f.tmp" "$f"; done
```

Создать `test/traces/.gitkeep` пустым, чтобы каталог был в репозитории даже без трасс.

- [ ] **Step 8: Прогнать тесты на трассах**

```bash
pio test -e native -f test_shake
```

Ожидается: `test_recorded_traces` больше не IGNORE, а PASSED.

Если трасса падения или шлепка даёт события — это калибровка, а не баг кода: подобрать `kImpactG`, `kFreefallG`, `kFreefallMs`, `kSuppressMs` в `src/Config.h`, перезапуская только нативные тесты (прошивать не нужно — в этом весь смысл разделения, §7). Если трасса тряски даёт ноль событий — понижать `kShakeOn`. Каждое изменение константы фиксировать в `docs/hw-notes.md` со старым и новым значением.

- [ ] **Step 9: Записать результаты калибровки**

Дописать в `docs/hw-notes.md`:

```markdown
## Калибровка по трассам (Task 7)

Дата: <YYYY-MM-DD>

| Константа | Значение в спецификации | Принятое значение | Основание |
|---|---|---|---|
| `kTiltAxis` | 0 (стартовое) | <0/1/2> | размах угла между вертикальными положениями |
| `kShakeOn` | 0,35 | <> | трасса baby_shake.csv |
| `kImpactG` | 3,5 | <> | трассы drop_table.csv, slap_table.csv |
| `kFreefallG` | 0,3 | <> | трасса drop_table.csv |
| `kSuppressMs` | 400 | <> | гашение отскока в drop_table.csv |

Трассы: `test/traces/` (rest_sofa, baby_shake, drop_table, slap_table).
```

- [ ] **Step 10: Прогнать весь набор и собрать прошивку**

```bash
pio test -e native && pio run -e m5stack-stickc-plus2
```

- [ ] **Step 11: Коммит**

```bash
git add platformio.ini src/Accel.h src/Accel.cpp src/trace_main.cpp src/Config.h test/traces docs/hw-notes.md
git commit -m "feat: IMU layer over ShakeDetector, trace firmware and threshold calibration"
```

---

### Task 8: `Display` — полоса, индикаторы, тело экрана

**Files:**
- Create: `src/Display.h`, `src/Display.cpp`
- Modify: `src/main.cpp` (временный визуальный пробник; в Task 9 заменяется штатным циклом)

**Interfaces:**
- Consumes: `kBarHeight`, `kScreenWidth`, `kScreenHeight`, `kMaxBrightness`, пороги батареи, `VolumeLevel` из `src/Config.h`.
- Produces:
  ```cpp
  class Display {
   public:
    void begin();
    void setTitle(const char* name, uint16_t color);
    void setBattery(uint8_t percent, bool charging);
    void setVolume(VolumeLevel v);
    void setFault(bool imuFailed);
    void fillBody(uint16_t color);
    void fillBodyRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
    void clearBody();
    void drawBodyMessage(const char* text, uint16_t color);
    void setBrightness(uint8_t v);          // зажимается kMaxBrightness
    int16_t bodyTop() const;                // = kBarHeight
    int16_t bodyHeight() const;             // = kScreenHeight - kBarHeight
    int16_t width() const;                  // = kScreenWidth
    static uint16_t hsv(float h, float s, float v);   // h в градусах, s/v 0..1
  };
  ```
  Координаты тела в `fillBodyRect` отсчитываются от `bodyTop()`, то есть `y = 0` — верх тела, а не верх экрана.

- [ ] **Step 1: Написать `src/Display.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Config.h"

// Единственный владелец M5.Display. Полоса сверху + тело экрана.
class Display {
 public:
  void begin();

  void setTitle(const char* name, uint16_t color);
  void setBattery(uint8_t percent, bool charging);
  void setVolume(VolumeLevel v);
  void setFault(bool imuFailed);           // §12: красный маркер IMU

  void fillBody(uint16_t color);
  void fillBodyRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c);
  void clearBody();
  void drawBodyMessage(const char* text, uint16_t color);

  void setBrightness(uint8_t v);           // зажимается kMaxBrightness

  int16_t bodyTop()    const { return kBarHeight; }
  int16_t bodyHeight() const { return kScreenHeight - kBarHeight; }
  int16_t width()      const { return kScreenWidth; }

  static uint16_t hsv(float h, float s, float v);

 private:
  void drawBar();

  const char* _title      = "";
  uint16_t    _titleColor = 0xFFFF;
  uint8_t     _percent    = 100;
  bool        _charging   = false;
  VolumeLevel _volume     = VolumeLevel::Quiet;
  bool        _fault      = false;
  uint8_t     _brightness = kMaxBrightness;
};
```

- [ ] **Step 2: Написать `src/Display.cpp`**

```cpp
#include "Display.h"

#include <M5Unified.h>

namespace {

constexpr uint16_t kBlack = 0x0000;

// Геометрия индикаторов (§9).
constexpr int16_t kVolX = 95, kVolY = 8, kVolW = 12, kVolH = 11;
constexpr int16_t kBatX = 109, kBatY = 8, kBatW = 22, kBatH = 11;

inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return lgfx::color565(r, g, b);
}

}  // namespace

void Display::begin() {
  M5.Display.setRotation(0);                 // портрет 135x240
  M5.Display.setBrightness(kMaxBrightness);
  M5.Display.fillScreen(kBlack);
  // efont: кириллица в UTF-8 без внешних шрифтов и таблиц CP1251 (§9).
  M5.Display.setFont(&lgfx::v1::fonts::efontCN_14);
  M5.Display.setTextSize(1);
  M5.Display.setTextWrap(false);
  _brightness = kMaxBrightness;
  drawBar();
}

uint16_t Display::hsv(float h, float s, float v) {
  while (h < 0.0f) h += 360.0f;
  while (h >= 360.0f) h -= 360.0f;
  if (s < 0.0f) s = 0.0f;
  if (s > 1.0f) s = 1.0f;
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;

  const float c = v * s;
  const float hp = h / 60.0f;
  const int   sector = (int)hp;
  const float f = hp - (float)sector;
  const float x = c * (1.0f - f);
  const float y = c * f;
  const float m = v - c;

  float r = m, g = m, b = m;
  switch (sector % 6) {
    case 0: r = v;     g = m + y; b = m;     break;
    case 1: r = m + x; g = v;     b = m;     break;
    case 2: r = m;     g = v;     b = m + y; break;
    case 3: r = m;     g = m + x; b = v;     break;
    case 4: r = m + y; g = m;     b = v;     break;
    default: r = v;    g = m;     b = m + x; break;
  }
  return rgb((uint8_t)(r * 255.0f), (uint8_t)(g * 255.0f), (uint8_t)(b * 255.0f));
}

void Display::setTitle(const char* name, uint16_t color) {
  if (_title == name && _titleColor == color) return;
  _title = name;
  _titleColor = color;
  drawBar();
}

void Display::setBattery(uint8_t percent, bool charging) {
  if (percent > 100) percent = 100;
  if (_percent == percent && _charging == charging) return;
  _percent = percent;
  _charging = charging;
  drawBar();
}

void Display::setVolume(VolumeLevel v) {
  if (_volume == v) return;
  _volume = v;
  drawBar();
}

void Display::setFault(bool imuFailed) {
  if (_fault == imuFailed) return;
  _fault = imuFailed;
  drawBar();
}

void Display::setBrightness(uint8_t v) {
  if (v > kMaxBrightness) v = kMaxBrightness;   // единственный зажим в проекте
  if (v == _brightness) return;
  _brightness = v;
  M5.Display.setBrightness(v);
}

void Display::fillBody(uint16_t color) {
  M5.Display.fillRect(0, bodyTop(), width(), bodyHeight(), color);
}

void Display::fillBodyRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
  if (y < 0) { h += y; y = 0; }
  if (h <= 0) return;
  if (y + h > bodyHeight()) h = bodyHeight() - y;
  if (h <= 0) return;
  M5.Display.fillRect(x, bodyTop() + y, w, h, c);
}

void Display::clearBody() {
  fillBody(kBlack);
}

void Display::drawBodyMessage(const char* text, uint16_t color) {
  clearBody();
  M5.Display.setTextColor(color, kBlack);
  const int32_t w = M5.Display.textWidth(text);
  const int32_t x = (width() - w) / 2;
  const int32_t y = bodyTop() + bodyHeight() / 2 - 8;
  M5.Display.setCursor(x < 0 ? 0 : x, y);
  M5.Display.print(text);
}

void Display::drawBar() {
  M5.Display.fillRect(0, 0, kScreenWidth, kBarHeight, kBlack);

  // Название режима. При отказе IMU полоса отдаётся маркеру (§12): имени
  // у аварийного режима нет, а причина должна быть видна сразу.
  if (_fault) {
    M5.Display.setTextColor(rgb(0xFF, 0x30, 0x30), kBlack);
    M5.Display.setCursor(4, 6);
    M5.Display.print("IMU");
  } else {
    M5.Display.setTextColor(_titleColor, kBlack);
    M5.Display.setCursor(4, 6);
    M5.Display.print(_title);
  }

  // Индикатор громкости: конус + 0/1/2 волны.
  M5.Display.fillRect(kVolX, kVolY + 3, 3, 5, 0xFFFF);
  M5.Display.fillTriangle(kVolX + 3, kVolY + 5,
                          kVolX + 6, kVolY,
                          kVolX + 6, kVolY + kVolH - 1, 0xFFFF);
  const uint8_t waves = (_volume == VolumeLevel::Normal) ? 2
                      : (_volume == VolumeLevel::Quiet)  ? 1 : 0;
  for (uint8_t i = 0; i < waves; ++i) {
    const int16_t x = kVolX + 8 + i * 2;
    M5.Display.drawFastVLine(x, kVolY + 3 - i, 5 + 2 * i, 0xFFFF);
  }
  if (waves == 0) {
    M5.Display.drawLine(kVolX + 8, kVolY + 2, kVolX + 11, kVolY + 8, rgb(0xFF, 0x30, 0x30));
    M5.Display.drawLine(kVolX + 11, kVolY + 2, kVolX + 8, kVolY + 8, rgb(0xFF, 0x30, 0x30));
  }

  // Индикатор заряда: контур + пропорциональная заливка + цветовая кодировка.
  const uint16_t battColor = (_percent < kBattCritPct) ? rgb(0xFF, 0x30, 0x30)
                           : (_percent < kBattLowPct)  ? rgb(0xFF, 0xC0, 0x20)
                                                       : rgb(0x40, 0xE0, 0x60);
  M5.Display.drawRect(kBatX, kBatY, kBatW - 2, kBatH, 0xFFFF);
  M5.Display.fillRect(kBatX + kBatW - 2, kBatY + 3, 2, 5, 0xFFFF);
  const int16_t inner = kBatW - 6;
  const int16_t fill = (int16_t)((int32_t)inner * _percent / 100);
  if (fill > 0) {
    M5.Display.fillRect(kBatX + 2, kBatY + 2, fill, kBatH - 4, battColor);
  }
  if (_charging) {
    const int16_t cx = kBatX + (kBatW - 2) / 2;
    M5.Display.drawLine(cx + 1, kBatY + 2, cx - 2, kBatY + 6, kBlack);
    M5.Display.drawLine(cx - 2, kBatY + 6, cx + 2, kBatY + 5, kBlack);
    M5.Display.drawLine(cx + 2, kBatY + 5, cx - 1, kBatY + 9, kBlack);
  }
}
```

- [ ] **Step 3: Написать визуальный пробник вместо заготовки `main.cpp`**

Полностью заменить `src/main.cpp` (в Task 9 он будет заменён штатным циклом):

```cpp
// Визуальный пробник Display. Заменяется штатным циклом в Task 9.
#include <M5Unified.h>

#include "Config.h"
#include "Display.h"

static Display display;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  display.begin();
  display.setTitle("Погремушка", Display::hsv(35.0f, 0.75f, 1.0f));
  display.setVolume(VolumeLevel::Quiet);
  display.setBattery(100, false);
  display.clearBody();
}

void loop() {
  static uint32_t phase = 0;
  static float hue = 0.0f;
  M5.update();
  const uint32_t now = millis();

  // Каждые 2 с меняем то, что показывает полоса.
  if (now / 2000 != phase) {
    phase = now / 2000;
    switch (phase % 6) {
      case 0: display.setTitle("Погремушка", Display::hsv(35.0f, 0.75f, 1.0f));
              display.setVolume(VolumeLevel::Quiet);
              display.setBattery(100, false); break;
      case 1: display.setTitle("Шкатулка", Display::hsv(250.0f, 0.4f, 1.0f));
              display.setVolume(VolumeLevel::Normal);
              display.setBattery(45, false); break;
      case 2: display.setTitle("Пианино", Display::hsv(145.0f, 0.6f, 0.9f));
              display.setVolume(VolumeLevel::Mute);
              display.setBattery(18, false); break;
      case 3: display.setBattery(8, false); break;
      case 4: display.setBattery(60, true); break;
      default:
        display.drawBodyMessage("Заряд", Display::hsv(0.0f, 1.0f, 1.0f));
        break;
    }
  }

  // Плавный свип тела: 8 с на полный круг — медленнее 0,5 Гц.
  if ((phase % 6) != 5) {
    hue += 360.0f * (float)kRenderMs / (float)kSweepPeriodFastMs;
    display.fillBody(Display::hsv(hue, 0.5f, 1.0f));
  }
  M5.delay(kRenderMs);
}
```

- [ ] **Step 4: Собрать и проверить размер прошивки**

```bash
pio run -e m5stack-stickc-plus2
```

Ожидается: SUCCESS. Шрифт efontCN_14 добавляет ~262 КБ; в отчёте должно остаться запас до 3 342 336 байт. Если размер превысил раздел — это разговор про шрифт, а не про остальной код: зафиксировать цифры и остановиться.

- [ ] **Step 5: Прошить и проверить глазами [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t upload
```

Проверить по списку:
1. Русские названия («Погремушка», «Шкатулка», «Пианино») читаются без «крокозябр» и не наезжают на индикаторы.
2. Индикатор громкости меняется: одна волна → две волны → перечёркнутый.
3. Индикатор заряда меняет заливку и цвет: зелёный (100, 45), жёлтый (18), красный (8).
4. В фазе `charging` внутри батарейки видна молния.
5. Тело экрана плавно переливается, мерцания нет.
6. Сообщение «Заряд» выводится по центру тела.
7. Полоса не перерисовывается постоянно (не мигает) — она обновляется только при смене фазы.

**Если русские глифы не рисуются:** шрифт выбран неверно. Проверить альтернативы в том же порядке: `efontJA_14`, `efontKR_14`. Зафиксировать выбранный в `docs/hw-notes.md` и в коде.

- [ ] **Step 6: Записать результат проверки**

Дописать в `docs/hw-notes.md`:

```markdown
## Экран и шрифт (Task 8)

Дата: <YYYY-MM-DD>

| Проверка | Результат |
|---|---|
| Кириллица рисуется шрифтом | <efontCN_14 / ...> |
| Название + два индикатора укладываются в 135 px | <да/нет> |
| Молния при подключённом USB видна | <да/нет> |
| Мерцания тела экрана нет | <да/нет> |
| Размер прошивки / лимит | <bytes> / 3342336 |
```

- [ ] **Step 7: Коммит**

```bash
git add src/Display.h src/Display.cpp src/main.cpp docs/hw-notes.md
git commit -m "feat: display bar with Cyrillic title, battery and volume indicators"
```

---

### Task 9: Тик-цикл, `Context` и режим «Погремушка» — первая играющая прошивка

**Files:**
- Create: `src/Game.h`
- Create: `src/games/Maraca.h`, `src/games/Maraca.cpp`
- Modify: `src/main.cpp` (полная замена визуального пробника штатным циклом)

**Interfaces:**
- Consumes: `Accel` (Task 7), `Buzzer` (Tasks 5–6), `Display` (Task 8).
- Produces:
  ```cpp
  struct Context {
    uint32_t now;
    uint32_t dtMs;
    bool     renderTick;     // истинно ~20 раз в секунду
    Accel&   accel;
    Buzzer&  buzzer;
    Display& display;
  };

  class Game {
   public:
    virtual ~Game() = default;
    virtual const char* name()      const = 0;
    virtual uint16_t    nameColor() const = 0;
    virtual void onEnter(Context&) = 0;
    virtual void onExit (Context&) = 0;
    virtual void update (Context&) = 0;
  };

  class Maraca : public Game { /* name() == "Погремушка" */ };
  ```

- [ ] **Step 1: Написать `src/Game.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Accel.h"
#include "Buzzer.h"
#include "Display.h"

// Режимы получают accel/buzzer/display и НЕ получают Power: политика сна
// целиком принадлежит Power, иначе любой режим мог бы незаметно запретить сон.
struct Context {
  uint32_t now;
  uint32_t dtMs;
  bool     renderTick;     // истинно ~20 раз в секунду
  Accel&   accel;
  Buzzer&  buzzer;
  Display& display;
};

class Game {
 public:
  virtual ~Game() = default;
  virtual const char* name()      const = 0;
  virtual uint16_t    nameColor() const = 0;
  virtual void onEnter(Context&) = 0;
  virtual void onExit (Context&) = 0;
  virtual void update (Context&) = 0;
};
```

- [ ] **Step 2: Написать `src/games/Maraca.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Game.h"

// «Погремушка»: нота и цвет на фронт тряски, досыпание нот пока трясут,
// затухание в чёрный по окончании.
class Maraca : public Game {
 public:
  const char* name()      const override { return "Погремушка"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

 private:
  void startBurst(Context& ctx);

  int8_t   _paletteIndex = -1;    // чтобы два соседних выбора не повторялись
  float    _hue          = 0.0f;
  float    _levelEnv     = 0.0f;  // огибающая яркости, tau = kMaracaLevelTauMs
  uint32_t _nextNoteMs   = 0;
  uint32_t _fadeStartMs  = 0;
  bool     _fading       = false;
  bool     _lit          = false;
};
```

- [ ] **Step 3: Написать `src/games/Maraca.cpp`**

```cpp
#include "Maraca.h"

#include <M5Unified.h>   // Arduino random() и lgfx::color565

#include "Config.h"

namespace {

// Честно случайный RGB даёт преимущественно грязно-коричневое: берём
// 12 насыщенных оттенков (§10).
inline float hueOf(uint8_t index) {
  return 360.0f / (float)kMaracaPaletteSize * (float)index;
}

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

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
  int8_t pick = (int8_t)random(kMaracaPaletteSize);
  if (pick == _paletteIndex) {
    pick = (int8_t)((pick + 1) % kMaracaPaletteSize);
  }
  _paletteIndex = pick;
  _hue = hueOf((uint8_t)pick);

  const float intensity = ctx.accel.startIntensity();
  const uint16_t dur = (uint16_t)lerp((float)kMaracaNoteMinMs,
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
      const uint32_t interval = (uint32_t)lerp((float)kMaracaMaxIntervalMs,
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
    const float value = lerp(kMaracaValueFloor, 1.0f, _levelEnv);
    ctx.display.fillBody(Display::hsv(_hue, 1.0f, value));
  }
}
```

- [ ] **Step 4: Заменить `src/main.cpp` штатным циклом**

```cpp
#include <M5Unified.h>

#include <esp_random.h>

#include "Accel.h"
#include "Buzzer.h"
#include "Config.h"
#include "Display.h"
#include "Game.h"
#include "games/Maraca.h"

static Accel   accel;
static Buzzer  buzzer;
static Display display;
static Maraca  maraca;

static uint32_t s_lastTickMs   = 0;
static uint32_t s_lastRenderMs = 0;
static uint32_t s_lastBattMs   = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  randomSeed(esp_random());

  display.begin();
  accel.begin();
  buzzer.begin();
  buzzer.setVolumeLevel(VolumeLevel::Quiet);

  display.setTitle(maraca.name(), maraca.nameColor());
  display.setVolume(VolumeLevel::Quiet);
  display.setBrightness(kMaxBrightness);

  const uint32_t now = millis();
  Context ctx{now, kTickMs, false, accel, buzzer, display};
  maraca.onEnter(ctx);
}

void loop() {
  const uint32_t now = millis();
  const uint32_t dtMs = (s_lastTickMs == 0) ? kTickMs : (now - s_lastTickMs);
  s_lastTickMs = now;

  M5.update();                                   // 1. кнопки, динамик, питание
  accel.update(now);                             // 2. IMU и фильтры
                                                 // 3. кнопки — Task 10

  const bool renderTick = (now - s_lastRenderMs) >= kRenderMs;
  if (renderTick) s_lastRenderMs = now;

  Context ctx{now, dtMs, renderTick, accel, buzzer, display};
  maraca.update(ctx);                            // 4. логика режима
  buzzer.update(now);                            // 5. продвижение нот
                                                 // 6. power.update — Task 11

  // Временно здесь: в Task 11 опрос батареи переезжает в Power.
  if (now - s_lastBattMs >= kBatteryPollMs) {
    s_lastBattMs = now;
    int32_t level = M5.Power.getBatteryLevel();
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    display.setBattery((uint8_t)level,
                       M5.Power.isCharging() == m5::Power_Class::is_charging);
  }

  // 7. добрать тик до kTickMs по измеренному времени
  const uint32_t spent = millis() - now;
  M5.delay(spent >= kTickMs ? 1 : (kTickMs - spent));
}
```

- [ ] **Step 5: Собрать прошивку**

```bash
pio run -e m5stack-stickc-plus2
```

Ожидается: SUCCESS.

- [ ] **Step 6: Прошить и проверить на железе [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t upload
```

Проверить по списку (§1 «Критерии успеха»):
1. Тряска даёт ноту **сразу**, на фронте: задержка не ощущается.
2. Пока трясут — ноты сыплются; чем сильнее, тем чаще.
3. Сильнее тряска → заметно другая длительность первой ноты.
4. Цвет тела меняется на каждую тряску и не повторяет предыдущий.
5. Яркость «дышит» в такт, но не стробит.
6. После остановки тело плавно темнеет примерно за 0,6 с.
7. Положить устройство на стол и шлёпнуть — ноты нет.
8. Сбросить устройство со стола на подушку — мелодии нет.

- [ ] **Step 7: Измерить фактический период тика [железо]**

Временно добавить в конец `loop()`:

```cpp
  static uint32_t worst = 0;
  if (dtMs > worst) { worst = dtMs; Serial.printf("worst dt=%lu\n", (unsigned long)worst); }
```

Прошить, трясти 20 с, снять показания в мониторе, затем **убрать** эту вставку. Ожидается максимум порядка 22–25 мс (кадр с перезаливкой тела). Если видно > 40 мс — отрисовка не укладывается в бюджет, записать цифру в `docs/hw-notes.md`.

- [ ] **Step 8: Коммит**

```bash
git add src/Game.h src/games src/main.cpp
git commit -m "feat: tick loop, game context and Maraca mode (first playable build)"
```

---

### Task 10: `GameManager` — переключение режимов, громкость, сохранение состояния

**Files:**
- Create: `src/GameManager.h`, `src/GameManager.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Game`, `Context` (Task 9), `Buzzer::setVolumeLevel` (Task 6), `Display::setTitle/setVolume/clearBody/drawBodyMessage` (Task 8).
- Produces:
  ```cpp
  struct SavedState { uint8_t gameIndex; VolumeLevel volume; };
  SavedState loadSavedState(bool wokeFromDeepSleep);

  class GameManager {
   public:
    void begin(Game* const* games, uint8_t count, const SavedState& saved, Context& ctx);
    void update(Context& ctx);
    void next(Context& ctx);          // короткое нажатие A
    void cycleVolume(Context& ctx);   // короткое нажатие B
    void tickPersist(uint32_t now);   // запись в NVS по дебаунсу
    void flushPersist();              // немедленная запись (перед сном)
    uint8_t     index()  const;
    VolumeLevel volume() const;
    const char* currentName() const;
  };
  ```

- [ ] **Step 1: Написать `src/GameManager.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Config.h"
#include "Game.h"

struct SavedState {
  uint8_t     gameIndex;
  VolumeLevel volume;
};

// Пробуждение из сна берёт состояние из RTC_DATA_ATTR (при совпадении
// магического числа), холодный старт — из NVS. Несовпадение магии
// трактуется как холодный старт (§12).
SavedState loadSavedState(bool wokeFromDeepSleep);

class GameManager {
 public:
  void begin(Game* const* games, uint8_t count, const SavedState& saved, Context& ctx);
  void update(Context& ctx);

  void next(Context& ctx);
  void cycleVolume(Context& ctx);

  void tickPersist(uint32_t now);
  void flushPersist();

  uint8_t     index()  const { return _index; }
  VolumeLevel volume() const { return _volume; }
  const char* currentName() const { return _games[_index]->name(); }

 private:
  void markDirty(uint32_t now);

  Game* const* _games = nullptr;
  uint8_t      _count = 0;
  uint8_t      _index = 0;
  VolumeLevel  _volume = VolumeLevel::Quiet;
  bool         _dirty = false;
  uint32_t     _dirtyAtMs = 0;
};
```

- [ ] **Step 2: Написать `src/GameManager.cpp`**

```cpp
#include "GameManager.h"

#include <M5Unified.h>
#include <Preferences.h>

namespace {

constexpr uint32_t kRtcMagic = 0x4D524131u;   // 'MRA1'
constexpr const char* kNvsNamespace = "maraca";
constexpr const char* kNvsKeyGame   = "game";
constexpr const char* kNvsKeyVolume = "vol";

RTC_DATA_ATTR uint32_t s_rtcMagic  = 0;
RTC_DATA_ATTR uint8_t  s_rtcIndex  = 0;
RTC_DATA_ATTR uint8_t  s_rtcVolume = 0;

inline VolumeLevel volumeFromRaw(uint8_t raw) {
  return (raw > (uint8_t)VolumeLevel::Mute) ? VolumeLevel::Quiet : (VolumeLevel)raw;
}

}  // namespace

SavedState loadSavedState(bool wokeFromDeepSleep) {
  if (wokeFromDeepSleep && s_rtcMagic == kRtcMagic) {
    return SavedState{s_rtcIndex, volumeFromRaw(s_rtcVolume)};
  }

  SavedState st{0, VolumeLevel::Quiet};        // §12: пустая или битая NVS
  Preferences prefs;
  if (prefs.begin(kNvsNamespace, true)) {
    st.gameIndex = prefs.getUChar(kNvsKeyGame, 0);
    st.volume    = volumeFromRaw(prefs.getUChar(kNvsKeyVolume,
                                                (uint8_t)VolumeLevel::Quiet));
    prefs.end();
  }
  return st;
}

void GameManager::begin(Game* const* games, uint8_t count,
                        const SavedState& saved, Context& ctx) {
  _games = games;
  _count = count;
  _index = (count == 0 || saved.gameIndex >= count) ? 0 : saved.gameIndex;
  _volume = saved.volume;
  _dirty = false;

  ctx.buzzer.setVolumeLevel(_volume);
  ctx.display.setVolume(_volume);
  ctx.display.setTitle(currentName(), _games[_index]->nameColor());
  ctx.display.clearBody();
  _games[_index]->onEnter(ctx);
}

void GameManager::update(Context& ctx) {
  _games[_index]->update(ctx);
}

void GameManager::next(Context& ctx) {
  ctx.buzzer.stop();
  _games[_index]->onExit(ctx);
  _index = (uint8_t)((_index + 1) % _count);

  // Полоса и тело готовятся до onEnter, чтобы отрисовка режима не стиралась.
  ctx.display.setTitle(currentName(), _games[_index]->nameColor());
  ctx.display.clearBody();
  _games[_index]->onEnter(ctx);

  // Двухнотный восходящий подтверждающий сигнал: это намеренное действие
  // взрослого, и оно требует подтверждения (§10).
  const Note confirm[2] = {
    {kConfirmStepLow,  kConfirmNoteMs, 0},
    {kConfirmStepHigh, kConfirmNoteMs, 0},
  };
  ctx.buzzer.play(confirm, 2);

  markDirty(ctx.now);
}

void GameManager::cycleVolume(Context& ctx) {
  switch (_volume) {
    case VolumeLevel::Quiet:  _volume = VolumeLevel::Normal; break;
    case VolumeLevel::Normal: _volume = VolumeLevel::Mute;   break;
    default:                  _volume = VolumeLevel::Quiet;  break;
  }
  ctx.buzzer.setVolumeLevel(_volume);
  ctx.display.setVolume(_volume);
  markDirty(ctx.now);
}

void GameManager::markDirty(uint32_t now) {
  // RTC-память — это RAM, писать в неё можно на каждое изменение.
  s_rtcMagic  = kRtcMagic;
  s_rtcIndex  = _index;
  s_rtcVolume = (uint8_t)_volume;

  _dirty = true;
  _dirtyAtMs = now;
}

void GameManager::tickPersist(uint32_t now) {
  if (!_dirty) return;
  if (now - _dirtyAtMs < kNvsDebounceMs) return;
  flushPersist();
}

void GameManager::flushPersist() {
  if (!_dirty) return;
  // Дебаунс обязателен: запись на каждое нажатие дала бы сотни циклов
  // стирания флеша за вечер (§10).
  Preferences prefs;
  if (prefs.begin(kNvsNamespace, false)) {
    prefs.putUChar(kNvsKeyGame, _index);
    prefs.putUChar(kNvsKeyVolume, (uint8_t)_volume);
    prefs.end();
  }
  _dirty = false;
}
```

- [ ] **Step 3: Подключить менеджер в `src/main.cpp`**

Правка 1 — заменить включения и статические объекты:

```cpp
#include "GameManager.h"
```

добавить после `#include "Game.h"`, а объявления объектов привести к виду:

```cpp
static Accel       accel;
static Buzzer      buzzer;
static Display     display;
static Maraca      maraca;
static GameManager manager;

static Game* const s_games[] = { &maraca };   // MusicBox и TiltPiano — Tasks 12, 13
static constexpr uint8_t kGameCount = sizeof(s_games) / sizeof(s_games[0]);

static uint32_t s_lastTickMs   = 0;
static uint32_t s_lastRenderMs = 0;
static uint32_t s_lastBattMs   = 0;
static uint32_t s_splashUntil  = 0;
```

Правка 2 — тело `setup()` после `buzzer.begin();`:

```cpp
  const SavedState saved = loadSavedState(false);   // wake cause — Task 11
  const uint32_t now = millis();
  Context ctx{now, kTickMs, false, accel, buzzer, display};
  manager.begin(s_games, kGameCount, saved, ctx);
  display.setBrightness(kMaxBrightness);

  // Сплэш — состояние с временной меткой, не delay(): цикл продолжает
  // крутиться, и первая тряска обрабатывается даже во время сплэша (§6).
  display.drawBodyMessage(manager.currentName(), s_games[manager.index()]->nameColor());
  s_splashUntil = now + kSplashMs;
```

Удалить из `setup()` строки `buzzer.setVolumeLevel(...)`, `display.setTitle(...)`, `display.setVolume(...)` и прямой вызов `maraca.onEnter(ctx)` — это делает `manager.begin()`.

Правка 3 — в `loop()` между `accel.update(now);` и вычислением `renderTick` вставить обработку кнопок:

```cpp
  // 3. Кнопки: A — смена режима, B — цикл громкости.
  Context btnCtx{now, dtMs, false, accel, buzzer, display};
  if (M5.BtnA.wasClicked()) manager.next(btnCtx);
  if (M5.BtnB.wasClicked()) manager.cycleVolume(btnCtx);
```

Правка 4 — заменить `maraca.update(ctx);` на:

```cpp
  manager.update(ctx);
  manager.tickPersist(now);
```

Правка 5 — отрисовка подавляется на время сплэша:

```cpp
  const bool splash = now < s_splashUntil;
  const bool renderTick = !splash && (now - s_lastRenderMs) >= kRenderMs;
```

- [ ] **Step 4: Собрать прошивку**

```bash
pio run -e m5stack-stickc-plus2
```

- [ ] **Step 5: Прошить и проверить на железе [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t upload
```

Проверить:
1. При включении ~0,8 с показывается название режима, потом экран отдаётся режиму.
2. Нажатие A переключает режим (пока он один — слышится подтверждающий двухнотный сигнал, название перерисовывается).
3. Нажатие B циклически меняет индикатор громкости: одна волна → две волны → перечёркнутый → одна волна.
4. На `Mute` тряска даёт цвет, но не звук.
5. Выключить устройство удержанием кнопки C (> 6 с без USB), включить снова: уровень громкости сохранился.
6. Побарабанить по B десяток раз подряд: через 3 с после последнего нажатия состояние пишется один раз (проверяется шагом 5 после серии нажатий).

- [ ] **Step 6: Проверить устойчивость к пустой NVS [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t erase && pio run -e m5stack-stickc-plus2 -t upload
```

Ожидается: устройство стартует с режима 0 и громкости `Quiet`, без зависания и без мусора в полосе.

- [ ] **Step 7: Коммит**

```bash
git add src/GameManager.h src/GameManager.cpp src/main.cpp
git commit -m "feat: game manager with mode switching, volume cycling and debounced NVS"
```

---

### Task 11: `Power` — простой, deep sleep, батарея

**Files:**
- Create: `src/Power.h`, `src/Power.cpp`
- Modify: `src/Display.h`, `src/Display.cpp` (добавить `sleep()`)
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `Display` (Task 8), `GameManager::flushPersist` (Task 10), результаты спайка Task 2 из `docs/hw-notes.md`.
- Produces:
  ```cpp
  enum class PowerState : uint8_t { Awake, Dimmed };

  class Power {
   public:
    using SleepPrepare = void (*)(void* user);
    void    begin();
    bool    wokeFromDeepSleep() const;
    void    setSleepPrepare(SleepPrepare cb, void* user);
    void    update(uint32_t now, bool activity, Display& display);
    bool    btnASuppressed() const;
    uint8_t batteryPercent() const;
    bool    charging() const;
    PowerState state() const;
  };
  ```
  и `void Display::sleep()`.

- [ ] **Step 1: Добавить `sleep()` в `Display`**

В `src/Display.h`, публичная секция (после `setBrightness`):

```cpp
  void sleep();                            // гасит панель перед deep sleep
```

В `src/Display.cpp`, в конец файла:

```cpp
void Display::sleep() {
  M5.Display.sleep();
}
```

- [ ] **Step 2: Написать `src/Power.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Config.h"
#include "Display.h"

enum class PowerState : uint8_t { Awake, Dimmed };

// Единственный владелец политики сна. Режимы его не видят, иначе любой
// режим мог бы незаметно запретить засыпание (§4).
class Power {
 public:
  using SleepPrepare = void (*)(void* user);

  void begin();

  bool wokeFromDeepSleep() const { return _wokeFromSleep; }

  // Вызывается перед засыпанием: остановить звук и сбросить NVS.
  void setSleepPrepare(SleepPrepare cb, void* user) { _prepare = cb; _user = user; }

  void update(uint32_t now, bool activity, Display& display);

  // Пробуждающее нажатие A не должно прилететь в GameManager как «переключи
  // режим»: флаг снимается только после того, как A увидена отпущенной (§6).
  bool btnASuppressed() const { return _suppressBtnA; }

  uint8_t    batteryPercent() const { return _percent; }
  bool       charging()       const { return _charging; }
  PowerState state()          const { return _state; }

 private:
  void pollBattery(uint32_t now, Display& display);
  void applyBrightness(Display& display);
  void enterDeepSleep(Display& display);

  SleepPrepare _prepare = nullptr;
  void*        _user    = nullptr;

  PowerState _state         = PowerState::Awake;
  bool       _wokeFromSleep = false;
  bool       _suppressBtnA  = false;
  bool       _charging      = false;
  uint8_t    _percent       = 100;
  uint8_t    _lowStreak     = 0;
  uint32_t   _lastActiveMs  = 0;
  uint32_t   _lastBattMs    = 0;
  uint32_t   _warnUntilMs   = 0;
  bool       _warning       = false;
};
```

- [ ] **Step 3: Написать `src/Power.cpp`**

```cpp
#include "Power.h"

#include <M5Unified.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

void Power::begin() {
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  _wokeFromSleep = (cause == ESP_SLEEP_WAKEUP_EXT0);
  _suppressBtnA  = _wokeFromSleep;
  _state         = PowerState::Awake;
  _lastActiveMs  = millis();
  _lastBattMs    = 0;
}

void Power::update(uint32_t now, bool activity, Display& display) {
  if (_suppressBtnA && !M5.BtnA.isPressed()) _suppressBtnA = false;

  if (activity) {
    _lastActiveMs = now;
    if (_state == PowerState::Dimmed) _state = PowerState::Awake;
  }

  pollBattery(now, display);

  if (_warning) {
    if (now >= _warnUntilMs) {
      // Для разряженного элемента полное снятие питания честнее
      // микроампер сна (§6).
      M5.Power.powerOff();
    }
    return;
  }

  const uint32_t idle = now - _lastActiveMs;
  if (_state == PowerState::Awake) {
    if (idle >= kIdleDimMs) _state = PowerState::Dimmed;
  } else if (!_charging && idle >= kIdleDimMs + kDimToSleepMs) {
    // Пока подключён USB, автосон не наступает: на отладке устройство
    // постоянно на кабеле (§6).
    enterDeepSleep(display);
    return;
  }

  applyBrightness(display);
}

void Power::pollBattery(uint32_t now, Display& display) {
  if (_lastBattMs != 0 && now - _lastBattMs < kBatteryPollMs) return;
  _lastBattMs = now;

  int32_t level = M5.Power.getBatteryLevel();
  if (level < 0) level = 0;          // §12: чтение вне 0..100 зажимается
  if (level > 100) level = 100;
  _percent  = (uint8_t)level;
  _charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);

  display.setBattery(_percent, _charging);

  // Один шумный отсчёт ADC не имеет права выключить игрушку (§6).
  if (_percent < kBattOffPct && !_charging) {
    if (_lowStreak < 255) ++_lowStreak;
  } else {
    _lowStreak = 0;
  }
  if (_lowStreak >= kBattOffCount && !_warning) {
    _warning = true;
    _warnUntilMs = now + kBattWarnMs;
    display.setBrightness(kMaxBrightness);
    display.drawBodyMessage("Заряд", lgfx::color565(0xFF, 0x30, 0x30));
  }
}

void Power::applyBrightness(Display& display) {
  uint8_t base = kMaxBrightness;
  if (_percent < kBattCritPct && !_charging) base = (uint8_t)(kMaxBrightness / 2);

  const uint8_t target = (_state == PowerState::Dimmed)
      ? (uint8_t)((uint16_t)base * kDimBrightnessPct / 100)
      : base;
  display.setBrightness(target);
}

void Power::enterDeepSleep(Display& display) {
  if (_prepare) _prepare(_user);     // остановить звук, сбросить NVS

  // Звука при засыпании нет: писк в момент, когда ребёнок успокоился,
  // работает против назначения игрушки (§6).
  for (int b = kMaxBrightness; b >= 0; b -= 16) {
    display.setBrightness((uint8_t)(b < 0 ? 0 : b));
    delay(20);
  }
  display.sleep();

  // Пробуждение только кнопкой A: ext0 поддерживает один RTC-пин, а ext1
  // не выражает «любая из трёх» для активно-низких кнопок (§6).
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, 0);

  // Пин HOLD берётся через M5.getPin, а не константой 4: знание о железе
  // остаётся в M5Unified. M5.Power.deepSleep() не вызывается — он настроил
  // бы собственный пин пробуждения и перебил бы нашу настройку на A.
  const gpio_num_t hold = (gpio_num_t)M5.getPin(m5::pin_name_t::power_hold);
  gpio_hold_en(hold);
  gpio_deep_sleep_hold_en();

  esp_deep_sleep_start();
}
```

Если спайк Task 2 показал, что этой последовательности недостаточно, применить здесь решение, записанное в `docs/hw-notes.md`, и отметить расхождение комментарием в коде.

- [ ] **Step 4: Подключить `Power` в `src/main.cpp`**

Правка 1 — добавить включение `#include "Power.h"` и объект:

```cpp
static Power power;
```

Правка 2 — в `setup()`: `power.begin();` первым после `M5.begin(cfg)`; затем

```cpp
  const SavedState saved = loadSavedState(power.wokeFromDeepSleep());
```

вместо прежней строки с `false`, и после `manager.begin(...)`:

```cpp
  power.setSleepPrepare([](void* user) {
    auto* m = static_cast<GameManager*>(user);
    buzzer.stop();
    m->flushPersist();
  }, &manager);
```

(лямбда без захвата приводится к указателю на функцию — динамическая память не нужна).

Правка 3 — сплэш показывается только на холодном старте:

```cpp
  if (!power.wokeFromDeepSleep()) {
    display.drawBodyMessage(manager.currentName(),
                            s_games[manager.index()]->nameColor());
    s_splashUntil = now + kSplashMs;
  }
```

Правка 4 — учёт флага подавления при нажатии A:

```cpp
  if (M5.BtnA.wasClicked() && !power.btnASuppressed()) manager.next(btnCtx);
```

Правка 5 — удалить временный блок опроса батареи (`s_lastBattMs`, `M5.Power.getBatteryLevel()`) и переменную `s_lastBattMs`, вместо него после `buzzer.update(now);`:

```cpp
  // 6. Простой, яркость, засыпание.
  // §7: простой питается msSinceMotion, а не только событиями тряски —
  // медленное шевеление ребёнком тоже активность.
  const bool activity = accel.shakeStarted() || accel.isShaking()
                     || accel.msSinceMotion(now) < kMotionActiveMs
                     || buzzer.isPlaying()
                     || M5.BtnA.wasPressed() || M5.BtnB.wasPressed();
  power.update(now, activity, display);
```

- [ ] **Step 5: Собрать прошивку**

```bash
pio run -e m5stack-stickc-plus2
```

- [ ] **Step 6: Проверить сон и пробуждение на железе [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t upload
```

Отключить USB (иначе автосон не наступает — это штатное поведение) и проверить:
1. Через 20 с покоя экран приглушается примерно до 10%, устройство остаётся отзывчивым: тряска сразу возвращает полную яркость.
2. Ещё через 60 с покоя экран гаснет, устройство замолкает, без звука при засыпании.
3. Нажатие A поднимает устройство, **режим не меняется** (критерий успеха 7).
4. Повторить трижды: режим стабилен, громкость сохранена.
5. Подключить USB и оставить на 2 минуты без движения: устройство не засыпает.
6. Оставить на батарее на ночь: утром устройство спит, а не разряжено (критерий успеха 6). Этот пункт выполняется отложенно; результат дописывается в заметки.

- [ ] **Step 7: Проверить индикацию и защиту по разряду [железо]**

1. Подключить USB: внутри батарейки появляется молния, цвет соответствует уровню.
2. Разрядить устройство до < 20%: индикатор жёлтый; до < 10%: красный и яркость заметно ниже.
3. Проверку выключения по 5 % не форсировать искусственно, но зафиксировать, что при реальном разряде появилось сообщение «Заряд» и устройство выключилось.

- [ ] **Step 8: Записать результат**

Дописать в `docs/hw-notes.md` раздел `## Power (Task 11)` с таблицей по пунктам шагов 6 и 7 (проверка / результат), включая отложенный ночной тест.

- [ ] **Step 9: Коммит**

```bash
git add src/Power.h src/Power.cpp src/Display.h src/Display.cpp src/main.cpp docs/hw-notes.md
git commit -m "feat: power states, deep sleep with HOLD and battery supervision"
```

---

### Task 12: Режим «Музыкальная шкатулка»

**Files:**
- Create: `src/games/MusicBox.h`, `src/games/MusicBox.cpp`
- Modify: `src/main.cpp` (добавить в массив режимов)

**Interfaces:**
- Consumes: `Game`, `Context` (Task 9), `Buzzer::push/pending/setTempoScale` (Tasks 5–6), `Display::hsv/fillBody` (Task 8).
- Produces: `class MusicBox : public Game` с `name() == "Шкатулка"`.

- [ ] **Step 1: Написать `src/games/MusicBox.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Game.h"

// «Шкатулка»: тряска заводит, фраза играет по кругу, завод убывает,
// музыка и свип оттенка замедляются синхронно.
class MusicBox : public Game {
 public:
  const char* name()      const override { return "Шкатулка"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

 private:
  float    windFraction() const;

  uint8_t  _wind        = 0;
  uint32_t _decayAccMs  = 0;
  float    _hue         = 0.0f;
};
```

- [ ] **Step 2: Написать `src/games/MusicBox.cpp`**

```cpp
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
```

- [ ] **Step 3: Добавить режим в `src/main.cpp`**

```cpp
#include "games/MusicBox.h"
```

```cpp
static MusicBox musicBox;

static Game* const s_games[] = { &maraca, &musicBox };   // TiltPiano — Task 13
```

- [ ] **Step 4: Собрать прошивку**

```bash
pio run -e m5stack-stickc-plus2
```

- [ ] **Step 5: Прошить и проверить на железе [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t upload
```

Нажать A, чтобы перейти в «Шкатулку», и проверить:
1. Без тряски музыки нет, тело экрана медленно переливается пастельными тонами и приглушено.
2. Одна тряска запускает фразу; шесть тряск дают примерно 24 с музыки.
3. По мере затихания музыка **замедляется**, а не ускоряется, и свип оттенка замедляется вместе с ней.
4. Мерцания нет: полный цикл цвета не быстрее 8 с.
5. Фраза звучит без щелчков между нотами.
6. На `Mute` свип продолжается, музыка беззвучна, и экран остаётся синхронным.

- [ ] **Step 6: Коммит**

```bash
git add src/games/MusicBox.h src/games/MusicBox.cpp src/main.cpp
git commit -m "feat: music box mode with wind counter and synchronized hue sweep"
```

---

### Task 13: Режим «Наклон-пианино»

**Files:**
- Create: `src/games/TiltPiano.h`, `src/games/TiltPiano.cpp`
- Modify: `src/main.cpp` (добавить в массив режимов)

**Interfaces:**
- Consumes: `Accel::tiltAngle/motionRate` (Task 7), `Buzzer::glide/stop/pitchOf` (Task 6), `Display::fillBodyRect/hsv` (Task 8).
- Produces: `class TiltPiano : public Game` с `name() == "Пианино"`.

- [ ] **Step 1: Написать `src/games/TiltPiano.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Game.h"

// «Пианино»: наклон задаёт ступень, звук идёт только пока устройство
// двигается. Полоса остаётся на экране и в тишине.
class TiltPiano : public Game {
 public:
  const char* name()      const override { return "Пианино"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

 private:
  void drawBar(Context& ctx, int16_t height, uint16_t color, bool full);

  int8_t   _step       = -1;
  int16_t  _prevHeight = 0;
  uint32_t _silentAtMs = 0;
  bool     _sounding   = false;
};
```

- [ ] **Step 2: Написать `src/games/TiltPiano.cpp`**

```cpp
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
```

- [ ] **Step 3: Добавить режим в `src/main.cpp`**

```cpp
#include "games/TiltPiano.h"
```

```cpp
static TiltPiano tiltPiano;

static Game* const s_games[] = { &maraca, &musicBox, &tiltPiano };
```

- [ ] **Step 4: Собрать прошивку**

```bash
pio run -e m5stack-stickc-plus2
```

- [ ] **Step 5: Прошить и проверить на железе [железо]**

```bash
pio run -e m5stack-stickc-plus2 -t upload
```

Перейти в «Пианино» нажатиями A и проверить:
1. Наклон меняет высоту тона дискретными ступенями, все звучат «в тон».
2. Полоса на экране растёт и убывает вместе с наклоном, риски видны.
3. Цвет полосы закреплён за ступенью: одна и та же нота всегда одного цвета.
4. При остановке звук затухает примерно через 0,25 с, а полоса остаётся.
5. Мерцания нет: цвет меняется только при смене ступени.
6. Порядок режимов по кругу: Погремушка → Шкатулка → Пианино → Погремушка.

- [ ] **Step 6: Коммит**

```bash
git add src/games/TiltPiano.h src/games/TiltPiano.cpp src/main.cpp
git commit -m "feat: tilt piano mode with quantized pitch and step-colored bar"
```

---

### Task 14: Обработка ошибок и финальная приёмка

**Files:**
- Create: `src/games/Fallback.h`, `src/games/Fallback.cpp`
- Modify: `src/main.cpp`
- Modify: `docs/hw-notes.md`

**Interfaces:**
- Consumes: всё предыдущее.
- Produces:
  ```cpp
  class Fallback : public Game {
   public:
    void strike(Context& ctx);   // нота и вспышка по нажатию A
  };
  ```
  Аварийный путь в `main.cpp`: при `accel.isEnabled() == false` режимы не переключаются, полоса показывает красный маркер `IMU`.

- [ ] **Step 1: Написать `src/games/Fallback.h`**

```cpp
#pragma once
#include <stdint.h>

#include "Game.h"

// §12: при отказе IMU устройство не выглядит кирпичом — нота играется
// по нажатию A, а причина видна в полосе.
class Fallback : public Game {
 public:
  const char* name()      const override { return "IMU"; }
  uint16_t    nameColor() const override;

  void onEnter(Context& ctx) override;
  void onExit (Context& ctx) override;
  void update (Context& ctx) override;

  void strike(Context& ctx);

 private:
  uint32_t _flashUntilMs = 0;
  int16_t  _step         = 0;
  bool     _lit          = false;
};
```

- [ ] **Step 2: Написать `src/games/Fallback.cpp`**

```cpp
#include "Fallback.h"

#include <M5Unified.h>   // lgfx::color565

#include "Config.h"

uint16_t Fallback::nameColor() const {
  return lgfx::color565(0xFF, 0x30, 0x30);
}

void Fallback::onEnter(Context& ctx) {
  _flashUntilMs = 0;
  _step = 0;
  _lit = false;
  ctx.display.clearBody();
}

void Fallback::onExit(Context& ctx) {
  ctx.buzzer.stop();
  ctx.display.clearBody();
}

void Fallback::strike(Context& ctx) {
  const Note n{_step, kMaracaNoteMaxMs, 0};
  ctx.buzzer.play(&n, 1);
  _step = (int16_t)((_step + 2) % (kStepMax + 1));
  _flashUntilMs = ctx.now + 300;
  _lit = false;
}

void Fallback::update(Context& ctx) {
  if (!ctx.renderTick) return;

  const bool shouldLight = ctx.now < _flashUntilMs;
  if (shouldLight == _lit) return;      // одна заливка на событие, не мерцание
  _lit = shouldLight;
  ctx.display.fillBody(shouldLight ? Display::hsv(0.0f, 0.6f, 0.7f) : 0x0000);
}
```

- [ ] **Step 3: Подключить аварийный путь в `src/main.cpp`**

Правка 1 — включение и объект:

```cpp
#include "games/Fallback.h"
```

```cpp
static Fallback fallback;
static bool s_imuFault = false;
```

Правка 2 — в `setup()`, после `accel.begin();`:

```cpp
  s_imuFault = !accel.isEnabled();
  display.setFault(s_imuFault);
```

и выбор состава режимов перед `manager.begin(...)`:

```cpp
  Game* const* games = s_games;
  uint8_t count = kGameCount;
  if (s_imuFault) {
    static Game* const faultGames[] = { &fallback };
    games = faultGames;
    count = 1;
  }
  manager.begin(games, count, saved, ctx);
```

(при `s_imuFault` в `saved.gameIndex` может лежать индекс > 0 — `GameManager::begin` уже зажимает его по `count`.)

Правка 3 — обработка кнопок учитывает аварийный режим:

```cpp
  if (M5.BtnA.wasClicked() && !power.btnASuppressed()) {
    if (s_imuFault) fallback.strike(btnCtx);
    else            manager.next(btnCtx);
  }
```

Правка 4 — в аварийном режиме активность считается по кнопкам и звуку:

```cpp
  const bool activity = (!s_imuFault && (accel.shakeStarted() || accel.isShaking()
                                         || accel.msSinceMotion(now) < kMotionActiveMs))
                     || buzzer.isPlaying()
                     || M5.BtnA.wasPressed() || M5.BtnB.wasPressed();
```

- [ ] **Step 4: Проверить аварийный режим без железа с отказом IMU [железо]**

Временно подменить в `src/Accel.cpp` строку `_enabled = M5.Imu.isEnabled();` на `_enabled = false;`, собрать, прошить и проверить:

1. В полосе красный маркер `IMU`.
2. Нажатие A играет ноту и даёт одну вспышку тела, мерцания нет.
3. Нажатие B по-прежнему меняет громкость.
4. Устройство засыпает по простою и просыпается по A.

Затем **вернуть** строку обратно:

```bash
git checkout src/Accel.cpp && pio run -e m5stack-stickc-plus2
```

- [ ] **Step 5: Проверить работу без динамика (§12)**

`M5.Speaker.tone()` на выключенном динамике возвращает `false` и ничего не играет, поэтому отдельного кода не требуется. Проверка: в `src/main.cpp`, в `setup()` временно добавить `cfg.internal_spk = false;` перед `M5.begin(cfg);`, прошить и убедиться, что все три режима работают молча и визуализация сохраняется, после чего вернуть файл:

```bash
git checkout src/main.cpp && pio run -e m5stack-stickc-plus2
```

- [ ] **Step 6: Прогнать полный набор тестов и измерить размер прошивки**

```bash
pio test -e native && pio run -e m5stack-stickc-plus2
```

Ожидается: все нативные тесты PASSED; `Flash: ... bytes from 3342336 bytes` — с запасом (критерий §11.3, пункт 8). Зафиксировать точную цифру.

- [ ] **Step 7: Проверить, что границы владения железом не нарушены**

```bash
grep -rn "M5.Imu" src/ | grep -v "^src/Accel.cpp" | grep -v "^src/trace_main.cpp"
grep -rn "M5.Speaker" src/ | grep -v "^src/Buzzer.cpp"
grep -rn "M5.Display" src/ | grep -v "^src/Display.cpp" | grep -v "^src/trace_main.cpp"
grep -rn "setVolume\|setBrightness" src/ | grep -vE "^src/(Buzzer|Display|Power)\.(h|cpp)"
```

Ожидается: первые три команды — пустой вывод (кроме явно исключённых файлов); четвёртая — только вызовы `display.setBrightness(...)` из `Power.cpp` и `main.cpp`, то есть через `Display`, без прямых обращений к `M5.Display.setBrightness`.

- [ ] **Step 8: Пройти приёмочный список §11.3 [железо]**

Прошить финальную сборку и пройти список целиком, отмечая результат:

1. Пин HOLD держится в deep sleep: устройство засыпает, а не выключается.
2. Пробуждение кнопкой A не переключает режим.
3. Индикатор заряда сходится с реальным; молния появляется при подключении USB.
4. Громкость на 30 см от уха приемлема; при сомнении снизить `kMaxMasterVolume` и пересобрать.
5. Ни в одном режиме нет видимого мерцания тела экрана.
6. Падение с высоты стола не вызывает мелодию.
7. Задержка от начала тряски до первой ноты не ощущается.
8. Прошивка укладывается в APP-раздел 3 342 336 байт вместе с efont.

- [ ] **Step 9: Записать итоги приёмки**

Дописать в `docs/hw-notes.md`:

```markdown
## Приёмка (Task 14)

Дата: <YYYY-MM-DD>
Размер прошивки: <bytes> из 3342336

| № | Проверка §11.3 | Результат |
|---|---|---|
| 1 | HOLD держится в deep sleep | <> |
| 2 | Пробуждение A не меняет режим | <> |
| 3 | Индикатор заряда и молния | <> |
| 4 | Громкость на 30 см | <> |
| 5 | Отсутствие мерцания | <> |
| 6 | Падение не вызывает мелодию | <> |
| 7 | Задержка отклика не ощущается | <> |
| 8 | Размер прошивки | <> |

Аварийные пути: отказ IMU — <>, отсутствие динамика — <>, пустая NVS — <>.
Константы, изменённые по итогам калибровки: <перечислить или «нет»>.
```

- [ ] **Step 10: Коммит**

```bash
git add src/games/Fallback.h src/games/Fallback.cpp src/main.cpp docs/hw-notes.md
git commit -m "feat: IMU-failure fallback mode and hardware acceptance results"
```

---

## Самопроверка плана относительно спецификации

| Раздел спецификации | Где реализуется |
|---|---|
| §1 Критерии успеха 1–3, 7 | Task 9 шаг 6, Task 11 шаг 6, Task 3/4 (тесты отклика и падений) |
| §1 Критерии успеха 4–6 | Task 6 (потолок громкости), Task 8/12/13 (мерцание), Task 11 (сон) |
| §2 Целевое железо, пинаут | Task 1 (описание платы), Task 2 (HOLD, кнопка A), Task 11 |
| §3 Конфигурация сборки | Task 1 целиком, Task 7 (окружение `trace`) |
| §4 Архитектура, границы владения | Global Constraints, Task 14 шаг 7 |
| §5 Тик-цикл, порядок операций, `dt` | Task 9 шаг 4, Task 3 (тест `dt`-независимости), Task 9 шаг 7 |
| §6 Состояния `Power`, сон, батарея | Task 2, Task 11 |
| §7 `Accel` и `ShakeDetector` | Tasks 3, 4, 7 |
| §8 `Buzzer` | Tasks 5, 6 |
| §9 `Display` | Task 8, Task 11 шаг 1 (`sleep`) |
| §10 Режимы и `GameManager` | Tasks 9, 10, 12, 13 |
| §11 Тестирование | Tasks 1, 3, 4, 5, 6 (нативные), Task 7 (трассы), Task 14 шаг 8 |
| §12 Обработка ошибок | Task 6 (`Mute`), Task 10 (NVS, магия RTC), Task 11 (зажим заряда), Task 5 (переполнение буфера), Task 14 (IMU, динамик) |
| §13 Константы | Task 1 (`src/Config.h`), Task 7 (калибровка) |
| §14 Порядок реализации и риски | Task 2 (спайк сна) — вторая задача после сборки; Task 7 (трассы для разделения тряски и удара) |
