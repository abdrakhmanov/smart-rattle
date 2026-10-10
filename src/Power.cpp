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
      // Намеренное выключение — такое же «перед сном», как и обычный deep
      // sleep: иначе смена громкости/режима за последние kNvsDebounceMs
      // теряется и динамик не остановлен (final review, Important 2).
      if (_prepare) _prepare(_user);
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
    // Яркость предупреждения: при критическом заряде тоже вдвое, как и
    // обычная подсветка — не стоит пережигать последние проценты ради
    // двух секунд полной яркости (final review, Minor).
    const uint8_t warnBrightness = (_percent < kBattCritPct && !_charging)
        ? (uint8_t)(kMaxBrightness / 2) : kMaxBrightness;
    display.setBrightness(warnBrightness);
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
  // Старт фейда — от ТЕКУЩЕЙ яркости, а не от kMaxBrightness: если экран
  // уже притушен (Dimmed) или ограничен вдвое (критический заряд), иначе
  // засыпание на миг вспыхивает полной яркостью (final review, Minor).
  for (int b = display.brightness(); b >= 0; b -= 16) {
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
  //
  // НЕВЕРИФИЦИРОВАНО НА ЖЕЛЕЗЕ: спайк Task 2, который должен был
  // подтвердить, что gpio_hold_en()+gpio_deep_sleep_hold_en() удерживает
  // питание в deep sleep, был отложен пользователем. docs/hw-notes.md не
  // содержит записи на этот счёт, поэтому используется базовая
  // последовательность из плана как единственный доступный вариант.
  const gpio_num_t hold = (gpio_num_t)M5.getPin(m5::pin_name_t::power_hold);
  gpio_hold_en(hold);
  gpio_deep_sleep_hold_en();

  esp_deep_sleep_start();
}
