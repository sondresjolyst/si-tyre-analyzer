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

inline Rgb heatRgb(float t, float lo, float hi) {
  const float span = hi - lo;
  float f = (span <= 0.0f) ? 0.0f : (t - lo) / span;
  if (f < 0.0f) f = 0.0f;
  if (f > 1.0f) f = 1.0f;
  const float r = 255.0f * (f * 2.0f < 1.0f ? f * 2.0f : 1.0f);
  const float b = 255.0f * ((1.0f - f) * 2.0f < 1.0f ? (1.0f - f) * 2.0f : 1.0f);
  float d = f - 0.5f;
  if (d < 0.0f) d = -d;
  float g = 80.0f * (1.0f - d * 2.0f);
  if (g < 0.0f) g = 0.0f;
  return Rgb{static_cast<uint8_t>(r + 0.5f), static_cast<uint8_t>(g + 0.5f),
             static_cast<uint8_t>(b + 0.5f)};
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
