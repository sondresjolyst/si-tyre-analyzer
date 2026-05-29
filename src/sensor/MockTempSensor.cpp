// Copyright (c) 2026 Sondre Sjølyst

#include "sensor/MockTempSensor.h"

#include <Arduino.h>
#include <math.h>

#include "sensor/SensorTypes.h"

namespace tyre {

MockTempSensor::MockTempSensor(float rateHz) : rateHz_(rateHz), startMs_(0) {}

bool MockTempSensor::begin() {
  startMs_ = millis();
  return true;
}

bool MockTempSensor::readFrame(float *out) {
  const float elapsedSec = (millis() - startMs_) / 1000.0f;

  // Warm-up curve: ~55 C rising toward ~85 C with a ~180 s time constant.
  const float ambient = 55.0f;
  const float peak = 85.0f;
  const float warm =
      ambient + (peak - ambient) * (1.0f - expf(-elapsedSec / 180.0f));

  // Roaming hot spot (simulated locked/overloaded patch) drifting across width.
  const float hotCol = (MLX_W - 1) * (0.5f + 0.45f * sinf(elapsedSec * 0.3f));
  const float hotRow = (MLX_H - 1) * (0.5f + 0.45f * cosf(elapsedSec * 0.2f));

  const float cx = (MLX_W - 1) / 2.0f;

  for (int r = 0; r < MLX_H; r++) {
    // Slight leading/trailing-edge gradient down the rotational axis.
    const float rowBias = ((r - (MLX_H - 1) / 2.0f) / (MLX_H - 1)) * 4.0f;

    for (int c = 0; c < MLX_W; c++) {
      // Tread profile: hot center, cooler shoulders (parabolic falloff).
      const float n = (c - cx) / cx;  // -1..1 across width
      const float treadFalloff = 12.0f * (n * n);

      float t = warm + rowBias - treadFalloff;

      // Roaming hot spot contribution (gaussian bump).
      const float dc = c - hotCol;
      const float dr = r - hotRow;
      t += 18.0f * expf(-(dc * dc + dr * dr) / 8.0f);

      // Per-pixel noise +/-1.5 C.
      t += ((random(0, 3001) / 1000.0f) - 1.5f);

      out[r * MLX_W + c] = t;
    }
  }
  return true;
}

}  // namespace tyre
