#include <unity.h>
#include "Config.h"
#include "signals.h"
#include "trace.h"

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

// Прямая проверка границы гейта свободного падения в State::Shaking:
// 2 подряд отсчёта mag < kFreefallG не должны подавлять, 3-й должен.
void test_freefall_gate_debounced_while_shaking() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedRest(d, c, 300);
  feedConstant(d, c, 0.8f, 100);   // вгоняем в State::Shaking
  TEST_ASSERT_TRUE(d.isShaking());

  // 2 подряд низких отсчёта (|a| ~ 0,04g < kFreefallG) — гейт не должен
  // сработать: тряска не прерывается.
  d.update(0.02f, -0.01f, 0.03f, 0.0f, 0.0f, 0.0f, 10);
  TEST_ASSERT_FALSE(d.suppressed());
  TEST_ASSERT_TRUE(d.isShaking());
  d.update(0.02f, -0.01f, 0.03f, 0.0f, 0.0f, 0.0f, 10);
  TEST_ASSERT_FALSE(d.suppressed());
  TEST_ASSERT_TRUE(d.isShaking());

  // 3-й подряд низкий отсчёт — гейт обязан сработать.
  d.update(0.02f, -0.01f, 0.03f, 0.0f, 0.0f, 0.0f, 10);
  TEST_ASSERT_TRUE(d.suppressed());
  TEST_ASSERT_FALSE(d.isShaking());
}

// Прямая проверка границы гейта в State::Idle: одного низкого отсчёта
// достаточно — задержки в несколько отсчётов, как в Shaking, быть не должно.
void test_freefall_gate_instant_while_idle() {
  ShakeDetector d; d.reset();
  EventCounts c;
  feedRest(d, c, 300);
  TEST_ASSERT_FALSE(d.isShaking());
  TEST_ASSERT_FALSE(d.suppressed());

  d.update(0.02f, -0.01f, 0.03f, 0.0f, 0.0f, 0.0f, 10);
  TEST_ASSERT_TRUE(d.suppressed());
  TEST_ASSERT_FALSE(d.isShaking());
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
  RUN_TEST(test_drop_from_table_yields_no_events);
  RUN_TEST(test_slap_on_table_yields_no_events);
  RUN_TEST(test_suppression_expires_and_shaking_works_again);
  RUN_TEST(test_tilt_angle_full_range);
  RUN_TEST(test_tilt_is_stable_under_shaking);
  RUN_TEST(test_motion_rate_is_gyro_magnitude);
  RUN_TEST(test_recorded_traces);
  RUN_TEST(test_freefall_gate_debounced_while_shaking);
  RUN_TEST(test_freefall_gate_instant_while_idle);
  return UNITY_END();
}
