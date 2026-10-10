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
