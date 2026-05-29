// Copyright (c) 2026 Sondre Sjølyst
// Native unit tests for versionCompare (semantic version ordering).
// Run with: pio test -e native_version

#include <unity.h>

#include "helpers/VersionUtils.h"

void setUp() {}
void tearDown() {}

void test_equal_versions() {
  TEST_ASSERT_EQUAL(0, versionCompare("v0.1.0", "v0.1.0"));
}

void test_newer_patch() {
  TEST_ASSERT_TRUE(versionCompare("v1.9.2", "v1.9.1") > 0);
}

void test_newer_minor_two_digits() {
  TEST_ASSERT_TRUE(versionCompare("v1.10.0", "v1.9.9") > 0);
}

void test_newer_major() {
  TEST_ASSERT_TRUE(versionCompare("v2.0.0", "v1.9.9") > 0);
}

void test_missing_v_prefix_treated_as_zero() {
  TEST_ASSERT_EQUAL(0, versionCompare("v0.0.0", "bad_input"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_equal_versions);
  RUN_TEST(test_newer_patch);
  RUN_TEST(test_newer_minor_two_digits);
  RUN_TEST(test_newer_major);
  RUN_TEST(test_missing_v_prefix_treated_as_zero);
  return UNITY_END();
}
