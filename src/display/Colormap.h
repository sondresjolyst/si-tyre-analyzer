// Copyright (c) 2026 Sondre Sjølyst

#ifndef SRC_DISPLAY_COLORMAP_H_
#define SRC_DISPLAY_COLORMAP_H_

#include <stdint.h>

namespace tyre {

struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct HeatStop {
  float f;
  uint8_t r, g, b;
};
inline Rgb heatRgb(float t, float lo, float hi) {
  static const HeatStop ramp[] = {{0.00f, 30, 70, 200},  {0.20f, 30, 165, 215},
                                  {0.40f, 40, 185, 80},  {0.62f, 70, 200, 70},
                                  {0.72f, 200, 210, 50}, {0.85f, 240, 150, 30},
                                  {1.00f, 215, 30, 30}};
  const int n = sizeof(ramp) / sizeof(ramp[0]);
  const float span = hi - lo;
  float f = (span <= 0.0f) ? 0.0f : (t - lo) / span;
  if (f < 0.0f)
    f = 0.0f;
  if (f > 1.0f)
    f = 1.0f;
  for (int i = 0; i < n - 1; i++) {
    if (f <= ramp[i + 1].f) {
      const HeatStop &a = ramp[i];
      const HeatStop &b = ramp[i + 1];
      const float u = (f - a.f) / (b.f - a.f);
      return Rgb{static_cast<uint8_t>(a.r + (b.r - a.r) * u + 0.5f),
                 static_cast<uint8_t>(a.g + (b.g - a.g) * u + 0.5f),
                 static_cast<uint8_t>(a.b + (b.b - a.b) * u + 0.5f)};
    }
  }
  return Rgb{ramp[n - 1].r, ramp[n - 1].g, ramp[n - 1].b};
}

inline uint16_t toRgb565(Rgb c) {
  return static_cast<uint16_t>(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) |
                               (c.b >> 3));
}

inline uint16_t heatRgb565(float t, float lo, float hi) {
  return toRgb565(heatRgb(t, lo, hi));
}

}  // namespace tyre

#endif  // SRC_DISPLAY_COLORMAP_H_
