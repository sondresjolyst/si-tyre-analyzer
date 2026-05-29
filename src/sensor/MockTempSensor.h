// Copyright (c) 2026 Sondre Sjølyst
// Synthetic MLX90640 frames for development before real sensors arrive.
// Models: a warm-up curve, a hot center tread band with cooler shoulders, a
// small leading/trailing row gradient, per-pixel noise, and a roaming hot spot.

#ifndef SRC_SENSOR_MOCKTEMPSENSOR_H_
#define SRC_SENSOR_MOCKTEMPSENSOR_H_

#include "sensor/ITempSensor.h"

namespace tyre {

class MockTempSensor : public ITempSensor {
 public:
  explicit MockTempSensor(float rateHz);
  bool begin() override;
  bool readFrame(float *out) override;
  float frameRateHz() const override { return rateHz_; }
  const char *name() const override { return "mock"; }

 private:
  float rateHz_;
  unsigned long startMs_;
};

}  // namespace tyre

#endif  // SRC_SENSOR_MOCKTEMPSENSOR_H_
