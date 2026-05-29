// Copyright (c) 2026 Sondre Sjølyst
// Hardware-independent thermal sensor interface. Implementations fill a full
// MLX_PIXELS frame (deg C, row-major); the shared downsample step reduces it.

#ifndef SRC_SENSOR_ITEMPSENSOR_H_
#define SRC_SENSOR_ITEMPSENSOR_H_

namespace tyre {

class ITempSensor {
 public:
  virtual ~ITempSensor() {}

  // Initialise the sensor (I2C/clock). Returns false on failure.
  virtual bool begin() = 0;

  // Fill `out` with MLX_PIXELS temperatures in deg C (row-major). Returns
  // false if the frame could not be read.
  virtual bool readFrame(float *out) = 0;

  // Configured frame rate in Hz.
  virtual float frameRateHz() const = 0;

  // Short identifier, e.g. "mock" / "mlx90640".
  virtual const char *name() const = 0;
};

}  // namespace tyre

#endif  // SRC_SENSOR_ITEMPSENSOR_H_
