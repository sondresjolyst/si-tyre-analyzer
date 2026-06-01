// Copyright (c) 2026 Sondre Sjølyst
// Native unit tests for the downsample bin-averaging + temp scaling.
// Run with: pio test -e native_downsample

#include <unity.h>

#include <cmath>
#include <vector>

// Module under test — include .cpp directly (build_src_filter is unreliable
// for native envs).
#include "processing/Downsample.cpp"  // NOLINT(build/include)

using tyre::applyFlip;
using tyre::downsample;
using tyre::scaleTemp;
using tyre::unscaleTemp;

void setUp() {}
void tearDown() {}

// Constant input -> every output cell equals the constant.
void test_constant_frame() {
  const int W = 8, H = 4;
  std::vector<float> in(W * H, 25.0f);
  float out[2 * 2];
  downsample(in.data(), W, H, out, 2, 2);
  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_FLOAT_WITHIN(1e-4, 25.0f, out[i]);
  }
}

// 4x4 -> 2x2: each output is the exact mean of its 2x2 block.
void test_block_mean() {
  // Row-major 4x4 where value = r*10 + c.
  float in[16];
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      in[r * 4 + c] = r * 10.0f + c;
  float out[4];
  downsample(in, 4, 4, out, 2, 2);
  // Top-left block: (0,0)(0,1)(1,0)(1,1) = 0,1,10,11 -> 5.5
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 5.5f, out[0]);
  // Top-right: 2,3,12,13 -> 7.5
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 7.5f, out[1]);
  // Bottom-left: 20,21,30,31 -> 25.5
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 25.5f, out[2]);
  // Bottom-right: 22,23,32,33 -> 27.5
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 27.5f, out[3]);
}

// NaN pixels are skipped; a fully-NaN block yields NaN.
void test_nan_skip() {
  float in[16];
  for (int i = 0; i < 16; i++)
    in[i] = 10.0f;
  // Make top-left block entirely NaN.
  in[0] = in[1] = in[4] = in[5] = NAN;
  // In top-right block, NaN one pixel; mean of remaining 3 (all 10) = 10.
  in[2] = NAN;
  float out[4];
  downsample(in, 4, 4, out, 2, 2);
  TEST_ASSERT_TRUE(std::isnan(out[0]));
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 10.0f, out[1]);
  TEST_ASSERT_FLOAT_WITHIN(1e-4, 10.0f, out[2]);
}

// 32x24 -> 6x3 (the production default): constant maps cleanly.
void test_mlx_to_6x3_constant() {
  const int W = 32, H = 24;
  std::vector<float> in(W * H, 70.0f);
  float out[6 * 3];
  downsample(in.data(), W, H, out, 6, 3);
  for (int i = 0; i < 18; i++) {
    TEST_ASSERT_FLOAT_WITHIN(1e-4, 70.0f, out[i]);
  }
}

// Hot center column should produce hotter middle output columns than edges.
void test_hot_center_profile() {
  const int W = 32, H = 24;
  std::vector<float> in(W * H, 60.0f);
  // Heat the central columns.
  for (int r = 0; r < H; r++)
    for (int c = 14; c <= 17; c++)
      in[r * W + c] = 90.0f;
  float out[6 * 3];
  downsample(in.data(), W, H, out, 6, 3);
  // Middle output column (index 2 or 3) hotter than edge columns (0 and 5).
  const float edgeL = out[0];        // row0,col0
  const float mid = out[0 * 6 + 3];  // row0,col3
  TEST_ASSERT_TRUE(mid > edgeL);
}

void test_scale_round_trip() {
  const float vals[] = {0.0f, 23.45f, -10.0f, 85.67f, 327.0f};
  for (float v : vals) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, v, unscaleTemp(scaleTemp(v)));
  }
}

// flipX mirrors columns, flipY mirrors rows; both together rotate 180.
void test_apply_flip() {
  // 3 cols x 2 rows, value = r*10 + c.
  auto fresh = [](float *g) {
    for (int r = 0; r < 2; r++)
      for (int c = 0; c < 3; c++)
        g[r * 3 + c] = r * 10.0f + c;
  };
  float g[6];

  fresh(g);
  applyFlip(g, 3, 2, false, false);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, g[0]);  // unchanged

  fresh(g);
  applyFlip(g, 3, 2, true, false);  // mirror columns
  TEST_ASSERT_EQUAL_FLOAT(2.0f, g[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, g[2]);
  TEST_ASSERT_EQUAL_FLOAT(12.0f, g[3]);

  fresh(g);
  applyFlip(g, 3, 2, false, true);  // mirror rows
  TEST_ASSERT_EQUAL_FLOAT(10.0f, g[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, g[3]);

  fresh(g);
  applyFlip(g, 3, 2, true, true);  // 180
  TEST_ASSERT_EQUAL_FLOAT(12.0f, g[0]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, g[5]);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_constant_frame);
  RUN_TEST(test_block_mean);
  RUN_TEST(test_nan_skip);
  RUN_TEST(test_mlx_to_6x3_constant);
  RUN_TEST(test_hot_center_profile);
  RUN_TEST(test_scale_round_trip);
  RUN_TEST(test_apply_flip);
  return UNITY_END();
}
