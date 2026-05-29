// Standalone clang++ validation of native-safe modules (Downsample, LogFormat).
// Not part of the build; used to verify logic where no gcc/Unity is available.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "processing/Downsample.cpp"
#include "storage/LogFormat.h"

using namespace tyre;

static int failures = 0;
#define CHECK(cond)                                            \
  do {                                                         \
    if (!(cond)) {                                             \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                              \
    }                                                          \
  } while (0)

static bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

int main() {
  // --- Downsample: constant ---
  {
    std::vector<float> in(8 * 4, 25.0f);
    float out[4];
    downsample(in.data(), 8, 4, out, 2, 2);
    for (int i = 0; i < 4; i++) CHECK(near(out[i], 25.0f, 1e-4f));
  }
  // --- Downsample: block mean 4x4 -> 2x2 ---
  {
    float in[16];
    for (int r = 0; r < 4; r++)
      for (int c = 0; c < 4; c++) in[r * 4 + c] = r * 10.0f + c;
    float out[4];
    downsample(in, 4, 4, out, 2, 2);
    CHECK(near(out[0], 5.5f, 1e-4f));
    CHECK(near(out[1], 7.5f, 1e-4f));
    CHECK(near(out[2], 25.5f, 1e-4f));
    CHECK(near(out[3], 27.5f, 1e-4f));
  }
  // --- Downsample: NaN skip ---
  {
    float in[16];
    for (int i = 0; i < 16; i++) in[i] = 10.0f;
    in[0] = in[1] = in[4] = in[5] = NAN;
    in[2] = NAN;
    float out[4];
    downsample(in, 4, 4, out, 2, 2);
    CHECK(std::isnan(out[0]));
    CHECK(near(out[1], 10.0f, 1e-4f));
  }
  // --- Downsample: 32x24 -> 6x3 constant + hot center ---
  {
    std::vector<float> in(32 * 24, 60.0f);
    for (int r = 0; r < 24; r++)
      for (int c = 14; c <= 17; c++) in[r * 32 + c] = 90.0f;
    float out[18];
    downsample(in.data(), 32, 24, out, 6, 3);
    CHECK(out[3] > out[0]);  // mid column hotter than edge
  }
  // --- scale round trip ---
  {
    const float vals[] = {0.0f, 23.45f, -10.0f, 85.67f};
    for (float v : vals) CHECK(near(unscaleTemp(scaleTemp(v)), v, 0.01f));
  }
  // --- LogFormat: header size + build/parse ---
  {
    CHECK(sizeof(LogHeader) == 64);
    CHECK(LOG_MAGIC == 0x54595254u);

    const int cols = 6, rows = 3, cells = cols * rows;
    LogHeader h{};
    h.magic = LOG_MAGIC;
    h.version = LOG_VERSION;
    h.grid_cols = cols;
    h.grid_rows = rows;
    h.sample_rate_hz = 4;
    h.wheel_pos = 1;
    h.temp_scale = kTempScale;
    h.session_id = 0xDEADBEEF;
    h.session_start_epoch_ms = 0;
    h.group_id = 0x1234;
    h.record_count = 0;

    const size_t stride = recordStride(cols, rows);
    CHECK(stride == (size_t)(4 + 2 * cells));

    // Serialise header + 2 records into a buffer, then parse back.
    std::vector<uint8_t> buf(sizeof(LogHeader) + 2 * stride, 0);
    std::memcpy(buf.data(), &h, sizeof(LogHeader));
    for (int rec = 0; rec < 2; rec++) {
      uint8_t *p = buf.data() + sizeof(LogHeader) + rec * stride;
      uint32_t t = (uint32_t)(rec * 250);
      std::memcpy(p, &t, 4);
      int16_t *temps = reinterpret_cast<int16_t *>(p + 4);
      for (int i = 0; i < cells; i++) temps[i] = scaleTemp(20.0f + rec + i);
    }

    LogHeader rh;
    std::memcpy(&rh, buf.data(), sizeof(LogHeader));
    CHECK(rh.magic == LOG_MAGIC);
    CHECK(rh.grid_cols == cols && rh.grid_rows == rows);
    CHECK(rh.session_id == 0xDEADBEEF);
    // recovered count from filesize when record_count==0
    size_t recovered = (buf.size() - sizeof(LogHeader)) / stride;
    CHECK(recovered == 2);
    // read back a temperature
    uint8_t *p1 = buf.data() + sizeof(LogHeader) + 1 * stride;
    int16_t *t1 = reinterpret_cast<int16_t *>(p1 + 4);
    CHECK(near(unscaleTemp(t1[0]), 21.0f, 0.01f));
  }

  if (failures == 0) {
    std::printf("ALL CHECKS PASSED\n");
    return 0;
  }
  std::printf("%d CHECK(S) FAILED\n", failures);
  return 1;
}
