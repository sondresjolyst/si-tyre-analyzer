// Copyright (c) 2026 Sondre Sjølyst

#include "sensor/Mlx90640Sensor.h"

#ifndef SENSOR_IS_MOCK

#include <Adafruit_MLX90640.h>
#include <Arduino.h>
#include <Wire.h>

#include "sensor/SensorTypes.h"

#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 18
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 17
#endif
#ifndef I2C_CLOCK_HZ
#define I2C_CLOCK_HZ 400000
#endif

namespace tyre {

static Adafruit_MLX90640 g_mlx;

static mlx90640_refreshrate_t refreshFromHz(float hz) {
  if (hz <= 1.0f)
    return MLX90640_1_HZ;
  if (hz <= 2.0f)
    return MLX90640_2_HZ;
  if (hz <= 4.0f)
    return MLX90640_4_HZ;
  if (hz <= 8.0f)
    return MLX90640_8_HZ;
  if (hz <= 16.0f)
    return MLX90640_16_HZ;
  return MLX90640_32_HZ;
}

Mlx90640Sensor::Mlx90640Sensor(float rateHz) : rateHz_(rateHz) {}

bool Mlx90640Sensor::begin() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);
  if (!g_mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {  // 0x33
    return false;
  }
  g_mlx.setMode(MLX90640_CHESS);
  g_mlx.setResolution(MLX90640_ADC_18BIT);
  g_mlx.setRefreshRate(refreshFromHz(rateHz_));
  return true;
}

bool Mlx90640Sensor::readFrame(float *out) {
  // getFrame fills MLX_PIXELS (768) floats in deg C; non-zero = read error.
  return g_mlx.getFrame(out) == 0;
}

}  // namespace tyre

#endif  // SENSOR_IS_MOCK
