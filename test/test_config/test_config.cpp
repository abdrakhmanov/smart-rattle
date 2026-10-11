#include <unity.h>
#include "Config.h"

void test_shake_thresholds_ordered() {
  TEST_ASSERT_TRUE(kShakeOff < kShakeOn);
  TEST_ASSERT_TRUE(kShakeOn < kShakeSat);
  TEST_ASSERT_TRUE(kShakeSat < kImpactG);
  TEST_ASSERT_TRUE(kMotionG < kShakeOff);
  TEST_ASSERT_TRUE(kFreefallG < 1.0f);
  TEST_ASSERT_TRUE(kFreefallDebounceShaking >= 1);
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
