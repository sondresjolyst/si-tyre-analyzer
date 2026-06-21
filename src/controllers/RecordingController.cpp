// Copyright (c) 2026 Sondre Sjølyst

#include "controllers/RecordingController.h"

#include <cstring>

#include "processing/Downsample.h"
#include "sensor/SensorTypes.h"
#include "storage/LogFormat.h"

#ifndef SESSION_MAX_SEC
#define SESSION_MAX_SEC 1800
#endif

namespace tyre {

bool RecordingController::start(uint32_t sessionId, uint64_t startEpochMs,
                                uint8_t wheel, const uint8_t mac[6],
                                const char *fwVer, uint32_t groupId,
                                const char *carName, uint8_t optLo,
                                uint8_t optHi) {
  const float rate = sensor_->frameRateHz();
  uint8_t flags = 0;
  if (flipX_)
    flags |= LOG_FLAG_FLIP_X;
  if (flipY_)
    flags |= LOG_FLAG_FLIP_Y;
  if (strcmp(sensor_->name(), "mock") == 0)
    flags |= LOG_FLAG_MOCK;
  if (!logger_->startSession(sessionId, startEpochMs, wheel,
                             static_cast<uint16_t>(rate), mac, fwVer, groupId,
                             carName, optLo, optHi, flags)) {
    return false;
  }
  sessionStartMs_ = millis();
  lastSampleMs_ = 0;
  lastFlushMs_ = sessionStartMs_;
  lastOffsetMs_ = 0;
  return true;
}

bool RecordingController::readAlignFrame(int16_t *out) {
  if (logger_->isRecording())  // never share the I2C bus with the sampler
    return false;
  if (!sensor_->readFrame(frame_))
    return false;
  applyFlip(frame_, MLX_W, MLX_H, flipX_, flipY_);
  for (int i = 0; i < MLX_PIXELS; i++)
    out[i] = scaleTemp(frame_[i]);
  return true;
}

void RecordingController::stop() {
  if (logger_->isRecording())
    logger_->endSession();
}

void RecordingController::tick() {
  if (!logger_->isRecording())
    return;

  const uint32_t now = millis();
  const uint32_t elapsed = now - sessionStartMs_;

  if (elapsed >= static_cast<uint32_t>(SESSION_MAX_SEC) * 1000UL) {
    stop();
    return;
  }

  const float rate = sensor_->frameRateHz();
  const uint32_t intervalMs = (rate > 0) ? (1000UL / rate) : 250UL;

  if (now - lastSampleMs_ >= intervalMs) {
    lastSampleMs_ = now;
    if (sensor_->readFrame(frame_)) {
      downsample(frame_, MLX_W, MLX_H, grid_, kGridCols, kGridRows);
      applyFlip(grid_, kGridCols, kGridRows, flipX_, flipY_);
      for (int i = 0; i < kGridCells; i++)
        scaled_[i] = scaleTemp(grid_[i]);
      lastOffsetMs_ = elapsed;
      logger_->appendSample(elapsed, scaled_);
    }
  }

  // Crash-resilience checkpoint every ~5 s.
  if (now - lastFlushMs_ >= 5000UL) {
    lastFlushMs_ = now;
    logger_->flush();
  }
}

}  // namespace tyre
