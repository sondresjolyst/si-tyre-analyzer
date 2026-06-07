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

// Optimal window drives the whole scale: cold 40%, optimal (green) 30%,
// high 15%, superhigh 15%. Scale ends derived from the window only.
inline float scaleLoC(float optLo, float optHi) {
  return optLo - 1.33333f * (optHi - optLo);
}
inline float scaleHiC(float optLo, float optHi) {
  return optHi + (optHi - optLo);
}

struct HeatStop {
  float t;
  uint8_t r, g, b;
};

inline Rgb heatRgb(float t, float optLo, float optHi) {
  float g = optHi - optLo;
  if (g <= 0.0f)
    g = 1.0f;
  const HeatStop ramp[] = {{optLo - 1.33333f * g, 30, 70, 200},
                           {optLo - 0.66667f * g, 30, 165, 215},
                           {optLo, 40, 185, 80},
                           {optLo + 0.6875f * g, 70, 200, 70},
                           {optHi, 200, 210, 50},
                           {optHi + 0.5f * g, 240, 150, 30},
                           {optHi + g, 215, 30, 30}};
  const int n = sizeof(ramp) / sizeof(ramp[0]);
  if (t <= ramp[0].t)
    return Rgb{ramp[0].r, ramp[0].g, ramp[0].b};
  for (int i = 0; i < n - 1; i++) {
    if (t <= ramp[i + 1].t) {
      const HeatStop &a = ramp[i];
      const HeatStop &b = ramp[i + 1];
      const float span = b.t - a.t;
      const float u = (span <= 0.0f) ? 0.0f : (t - a.t) / span;
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

inline uint16_t heatRgb565(float t, float optLo, float optHi) {
  return toRgb565(heatRgb(t, optLo, optHi));
}

}  // namespace tyre

#endif  // SRC_DISPLAY_COLORMAP_H_
