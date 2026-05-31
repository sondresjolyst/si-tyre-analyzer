// Copyright (c) 2026 Sondre Sjølyst

#include <unity.h>

#include "display/Colormap.h"

using tyre::heatRgb;
using tyre::Rgb;
using tyre::toRgb565;

void setUp() {}
void tearDown() {}

void test_cold_end() {
  Rgb c = heatRgb(0.0f, 0.0f, 100.0f);
  TEST_ASSERT_EQUAL_UINT8(0, c.r);
  TEST_ASSERT_EQUAL_UINT8(255, c.b);
}

void test_hot_end() {
  Rgb c = heatRgb(100.0f, 0.0f, 100.0f);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(0, c.b);
}

void test_mid() {
  Rgb c = heatRgb(50.0f, 0.0f, 100.0f);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(255, c.b);
  TEST_ASSERT_EQUAL_UINT8(80, c.g);
}

void test_clamps_below_range() {
  Rgb a = heatRgb(-20.0f, 0.0f, 100.0f);
  Rgb b = heatRgb(0.0f, 0.0f, 100.0f);
  TEST_ASSERT_EQUAL_UINT8(b.r, a.r);
  TEST_ASSERT_EQUAL_UINT8(b.b, a.b);
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
  RUN_TEST(test_cold_end);
  RUN_TEST(test_hot_end);
  RUN_TEST(test_mid);
  RUN_TEST(test_clamps_below_range);
  RUN_TEST(test_rgb565_packing);
  return UNITY_END();
}
