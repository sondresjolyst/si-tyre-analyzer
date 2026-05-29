// Copyright (c) 2026 Sondre Sjølyst

#include "processing/Downsample.h"

namespace tyre {

void downsample(const float *in, int inW, int inH, float *out, int outCols,
                int outRows) {
  for (int orow = 0; orow < outRows; orow++) {
    const int r0 = orow * inH / outRows;
    int r1 = (orow + 1) * inH / outRows;
    if (r1 > inH)
      r1 = inH;

    for (int ocol = 0; ocol < outCols; ocol++) {
      const int c0 = ocol * inW / outCols;
      int c1 = (ocol + 1) * inW / outCols;
      if (c1 > inW)
        c1 = inW;

      double sum = 0.0;
      int count = 0;
      for (int r = r0; r < r1; r++) {
        for (int c = c0; c < c1; c++) {
          const float v = in[r * inW + c];
          if (!std::isnan(v)) {
            sum += v;
            count++;
          }
        }
      }
      out[orow * outCols + ocol] =
          (count > 0) ? static_cast<float>(sum / count) : NAN;
    }
  }
}

}  // namespace tyre
