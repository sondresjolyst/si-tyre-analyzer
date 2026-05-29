// Copyright (c) 2026 Sondre Sjølyst
// Native unit tests for the binary log format (header size, stride, scaling,
// build/parse round-trip + record_count recovery from filesize).
// Run with: pio test -e native_logformat

#include <unity.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "processing/Downsample.h"
#include "storage/LogFormat.h"

using namespace tyre;

void setUp() {}
void tearDown() {}

void test_header_size_and_magic() {
  TEST_ASSERT_EQUAL_UINT32(96, sizeof(LogHeader));
  TEST_ASSERT_EQUAL_HEX32(0x54595254u, LOG_MAGIC);
}

void test_record_stride() {
  TEST_ASSERT_EQUAL_UINT32(4 + 2 * 6 * 3, recordStride(6, 3));
  TEST_ASSERT_EQUAL_UINT32(4 + 2 * 4 * 4, recordStride(4, 4));
}

void test_build_and_parse() {
  const int cols = 6, rows = 3, cells = cols * rows;
  const size_t stride = recordStride(cols, rows);

  LogHeader h{};
  h.magic = LOG_MAGIC;
  h.version = LOG_VERSION;
  h.grid_cols = cols;
  h.grid_rows = rows;
  h.sample_rate_hz = 4;
  h.wheel_pos = 1;
  h.temp_scale = kTempScale;
  h.session_id = 0xDEADBEEF;
  h.group_id = 0x1234;
  h.record_count = 0;  // not finalised

  std::vector<uint8_t> buf(sizeof(LogHeader) + 2 * stride, 0);
  std::memcpy(buf.data(), &h, sizeof(LogHeader));
  for (int rec = 0; rec < 2; rec++) {
    uint8_t *p = buf.data() + sizeof(LogHeader) + rec * stride;
    uint32_t t = static_cast<uint32_t>(rec * 250);
    std::memcpy(p, &t, 4);
    int16_t *temps = reinterpret_cast<int16_t *>(p + 4);
    for (int i = 0; i < cells; i++) temps[i] = scaleTemp(20.0f + rec + i);
  }

  LogHeader rh;
  std::memcpy(&rh, buf.data(), sizeof(LogHeader));
  TEST_ASSERT_EQUAL_HEX32(LOG_MAGIC, rh.magic);
  TEST_ASSERT_EQUAL_UINT8(cols, rh.grid_cols);
  TEST_ASSERT_EQUAL_UINT8(rows, rh.grid_rows);
  TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, rh.session_id);

  // record_count==0 -> recover from filesize
  size_t recovered = (buf.size() - sizeof(LogHeader)) / stride;
  TEST_ASSERT_EQUAL_UINT32(2, recovered);

  uint8_t *p1 = buf.data() + sizeof(LogHeader) + 1 * stride;
  int16_t *t1 = reinterpret_cast<int16_t *>(p1 + 4);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.0f, unscaleTemp(t1[0]));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_header_size_and_magic);
  RUN_TEST(test_record_stride);
  RUN_TEST(test_build_and_parse);
  return UNITY_END();
}
