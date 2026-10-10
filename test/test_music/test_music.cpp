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
  std::vector<float> glides;
  std::vector<uint8_t> volumes;

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
  void emitGlide(float hz) override { glides.push_back(hz); }
  void applyVolume(uint8_t v) override { volumes.push_back(v); }
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
  RUN_TEST(test_volume_levels_respect_ceiling);
  RUN_TEST(test_mute_keeps_sequence_running);
  RUN_TEST(test_glide_takes_over_and_cancels_queue);
  RUN_TEST(test_glide_reissues_only_on_pitch_change);
  RUN_TEST(test_push_cancels_glide);
  RUN_TEST(test_stop_ends_glide_and_playing);
  return UNITY_END();
}
