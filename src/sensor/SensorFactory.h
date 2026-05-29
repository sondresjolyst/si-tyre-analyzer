// Copyright (c) 2026 Sondre Sjølyst
// Compile-time selection of the sensor implementation. Only the selected
// driver is linked (SENSOR_IS_MOCK is set by extra_script.py for mock builds).

#ifndef SRC_SENSOR_SENSORFACTORY_H_
#define SRC_SENSOR_SENSORFACTORY_H_

#include "config/DeviceConfig.h"
#include "sensor/ITempSensor.h"

#ifdef SENSOR_IS_MOCK
#include "sensor/MockTempSensor.h"
#else
#include "sensor/Mlx90640Sensor.h"
#endif

namespace tyre {

inline ITempSensor &makeSensor() {
#ifdef SENSOR_IS_MOCK
  static MockTempSensor impl(SAMPLE_RATE_HZ);
#else
  static Mlx90640Sensor impl(SAMPLE_RATE_HZ);
#endif
  return impl;
}

}  // namespace tyre

#endif  // SRC_SENSOR_SENSORFACTORY_H_
