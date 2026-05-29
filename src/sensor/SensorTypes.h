// Copyright (c) 2026 Sondre Sjølyst
// Shared MLX90640 geometry constants. Pure header — native-test safe.

#ifndef SRC_SENSOR_SENSORTYPES_H_
#define SRC_SENSOR_SENSORTYPES_H_

namespace tyre {

// MLX90640 native thermal array. Width (32) maps to tyre tread; height (24)
// is the rotational/positional axis.
constexpr int MLX_W = 32;
constexpr int MLX_H = 24;
constexpr int MLX_PIXELS = MLX_W * MLX_H;  // 768

}  // namespace tyre

#endif  // SRC_SENSOR_SENSORTYPES_H_
