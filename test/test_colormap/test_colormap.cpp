// Copyright (c) 2026 Sondre Sjølyst

#include <unity.h>

#include "display/Colormap.h"

using tyre::heatRgb;
using tyre::Rgb;
using tyre::scaleHiC;
using tyre::scaleLoC;
using tyre::toRgb565;

void setUp() {}
void tearDown() {}

// Default historic window: opt 80-95 -> scale 60-110.
void test_scale_derived_from_window() {
  TEST_ASSERT_EQUAL_FLOAT(60.0f, scaleLoC(80.0f, 95.0f));
  TEST_ASSERT_EQUAL_FLOAT(110.0f, scaleHiC(80.0f, 95.0f));
}

void test_cold_end() {
  Rgb c = heatRgb(60.0f, 80.0f, 95.0f);
  TEST_ASSERT_EQUAL_UINT8(30, c.r);
  TEST_ASSERT_EQUAL_UINT8(70, c.g);
  TEST_ASSERT_EQUAL_UINT8(200, c.b);
}

void test_opt_low_is_green() {
  Rgb c = heatRgb(80.0f, 80.0f, 95.0f);
  TEST_ASSERT_EQUAL_UINT8(40, c.r);
  TEST_ASSERT_EQUAL_UINT8(185, c.g);
  TEST_ASSERT_EQUAL_UINT8(80, c.b);
}

void test_opt_high_is_yellow() {
  Rgb c = heatRgb(95.0f, 80.0f, 95.0f);
  TEST_ASSERT_EQUAL_UINT8(200, c.r);
  TEST_ASSERT_EQUAL_UINT8(210, c.g);
  TEST_ASSERT_EQUAL_UINT8(50, c.b);
}

void test_hot_end() {
  Rgb c = heatRgb(110.0f, 80.0f, 95.0f);
  TEST_ASSERT_EQUAL_UINT8(215, c.r);
  TEST_ASSERT_EQUAL_UINT8(30, c.g);
  TEST_ASSERT_EQUAL_UINT8(30, c.b);
}

void test_clamps_below_range() {
  Rgb a = heatRgb(40.0f, 80.0f, 95.0f);
  Rgb b = heatRgb(60.0f, 80.0f, 95.0f);
  TEST_ASSERT_EQUAL_UINT8(b.r, a.r);
  TEST_ASSERT_EQUAL_UINT8(b.g, a.g);
  TEST_ASSERT_EQUAL_UINT8(b.b, a.b);
}

// A shifted window re-centres the colours: 95 deg is the green edge, not the
// yellow it would be on the historic window.
void test_window_shifts_green() {
  Rgb c = heatRgb(95.0f, 95.0f, 105.0f);
  TEST_ASSERT_EQUAL_UINT8(40, c.r);
  TEST_ASSERT_EQUAL_UINT8(185, c.g);
  TEST_ASSERT_EQUAL_UINT8(80, c.b);
}

void test_rgb565_packing() {
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, toRgb565(Rgb{255, 255, 255}));
  TEST_ASSERT_EQUAL_UINT16(0x0000, toRgb565(Rgb{0, 0, 0}));
  TEST_ASSERT_EQUAL_UINT16(0xF800, toRgb565(Rgb{255, 0, 0}));
  TEST_ASSERT_EQUAL_UINT16(0x07E0, toRgb565(Rgb{0, 255, 0}));
  TEST_ASSERT_EQUAL_UINT16(0x001F, toRgb565(Rgb{0, 0, 255}));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_scale_derived_from_window);
  RUN_TEST(test_cold_end);
  RUN_TEST(test_opt_low_is_green);
  RUN_TEST(test_opt_high_is_yellow);
  RUN_TEST(test_hot_end);
  RUN_TEST(test_clamps_below_range);
  RUN_TEST(test_window_shifts_green);
  RUN_TEST(test_rgb565_packing);
  return UNITY_END();
}
