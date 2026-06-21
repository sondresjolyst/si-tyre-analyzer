// Copyright (c) 2026 Sondre Sjølyst
// Drives one wheel unit's recording: read frame -> downsample -> scale -> log,
// paced by the configured sample rate. Auto-stops at SESSION_MAX_SEC.

#ifndef SRC_CONTROLLERS_RECORDINGCONTROLLER_H_
#define SRC_CONTROLLERS_RECORDINGCONTROLLER_H_

#include <Arduino.h>

#include "config/DeviceConfig.h"
#include "sensor/ITempSensor.h"
#include "sensor/SensorTypes.h"
#include "storage/SessionLogger.h"

namespace tyre {

class RecordingController {
 public:
  RecordingController(ITempSensor *sensor, SessionLogger *logger)
      : sensor_(sensor), logger_(logger) {}

  bool beginSensor() { return sensor_->begin(); }

  // Mirror the grid to match how the sensor is mounted (see Downsample).
  void setFlip(bool flipX, bool flipY) {
    flipX_ = flipX;
    flipY_ = flipY;
  }

  // Start a session. wheel/mac/fwVer/groupId go into the file header.
  bool start(uint32_t sessionId, uint64_t startEpochMs, uint8_t wheel,
             const uint8_t mac[6], const char *fwVer, uint32_t groupId,
             const char *carName, uint8_t optLo, uint8_t optHi);

  void stop();

  // Call frequently from loop(); samples when the interval elapses.
  void tick();

  bool isRecording() const { return logger_->isRecording(); }

  // Latest downsampled grid (scaled), valid after a sample. For live streaming.
  const int16_t *lastScaledGrid() const { return scaled_; }
  uint32_t lastSampleOffsetMs() const { return lastOffsetMs_; }

  // Read one native-resolution frame (MLX_PIXELS, flip applied, scaled) for the
  // sensor-alignment view. Refuses (returns false) while recording so it never
  // races the sampler on the I2C bus; also false if the frame read fails.
  bool readAlignFrame(int16_t *out);

 private:
  ITempSensor *sensor_;
  SessionLogger *logger_;

  uint32_t sessionStartMs_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastFlushMs_ = 0;
  uint32_t lastOffsetMs_ = 0;

  bool flipX_ = false;
  bool flipY_ = false;

  float frame_[MLX_PIXELS];
  float grid_[kGridCells];
  int16_t scaled_[kGridCells];
};

}  // namespace tyre

#endif  // SRC_CONTROLLERS_RECORDINGCONTROLLER_H_
