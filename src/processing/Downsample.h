// Copyright (c) 2026 Sondre Sjølyst
// Bin-averages a full thermal frame into a small grid. Pure C++ (no Arduino)
// so it is unit-testable natively. Same routine feeds the logged grid and the
// ESP-NOW live grid.

#ifndef SRC_PROCESSING_DOWNSAMPLE_H_
#define SRC_PROCESSING_DOWNSAMPLE_H_

#include <stdint.h>

#include <cmath>

namespace tyre {

// Average input pixels (inW x inH, row-major, deg C) into out (outCols x
// outRows, row-major). Each output cell is the mean of the input block it
// covers; NaN input pixels are skipped. A fully-NaN block yields NaN.
// Caller supplies `out` (size outCols*outRows). No allocation.
void downsample(const float *in, int inW, int inH, float *out, int outCols,
                int outRows);

// Storage scaling: deg C <-> int16 hundredths.
constexpr int kTempScale = 100;

inline int16_t scaleTemp(float c) {
  float v = c * kTempScale;
  if (v > 32767.0f)
    v = 32767.0f;
  if (v < -32768.0f)
    v = -32768.0f;
  return static_cast<int16_t>(std::lroundf(v));
}

inline float unscaleTemp(int16_t v) {
  return static_cast<float>(v) / kTempScale;
}

}  // namespace tyre

#endif  // SRC_PROCESSING_DOWNSAMPLE_H_
