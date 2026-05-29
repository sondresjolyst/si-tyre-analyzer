// Copyright (c) 2026 Sondre Sjølyst
// Real Adafruit MLX90640 driver behind the ITempSensor interface.
// Body is compiled only when SENSOR_IS_MOCK is not defined.

#ifndef SRC_SENSOR_MLX90640SENSOR_H_
#define SRC_SENSOR_MLX90640SENSOR_H_

#include "sensor/ITempSensor.h"

namespace tyre {

class Mlx90640Sensor : public ITempSensor {
 public:
  explicit Mlx90640Sensor(float rateHz);
  bool begin() override;
  bool readFrame(float *out) override;
  float frameRateHz() const override { return rateHz_; }
  const char *name() const override { return "mlx90640"; }

 private:
  float rateHz_;
};

}  // namespace tyre

#endif  // SRC_SENSOR_MLX90640SENSOR_H_
