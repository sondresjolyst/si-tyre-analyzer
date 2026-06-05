// Copyright (c) 2026 Sondre Sjølyst
// Writes a recording session to LittleFS as one binary file. Buffers samples
// in RAM and flushes in batches so the sample loop never stalls on flash.

#ifndef SRC_STORAGE_SESSIONLOGGER_H_
#define SRC_STORAGE_SESSIONLOGGER_H_

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

#include <vector>

#include "config/DeviceConfig.h"
#include "storage/LogFormat.h"

namespace tyre {

struct SessionInfo {
  String name;    // file name (no dir)
  uint32_t size;  // bytes
  uint32_t session_id;
  uint8_t wheel;     // WheelPos
  uint32_t records;  // computed from size
};

class SessionLogger {
 public:
  bool begin();  // mount LittleFS, ensure /sessions

  // Open a new session file and write its header. wheel is WheelPos.
  bool startSession(uint32_t sessionId, uint64_t startEpochMs, uint8_t wheel,
                    uint16_t rateHz, const uint8_t mac[6], const char *fwVer,
                    uint32_t groupId, const char *carName, uint8_t optLo,
                    uint8_t optHi);

  // Append one downsampled grid. `temps` holds kGridCells scaled int16.
  bool appendSample(uint32_t tOffsetMs, const int16_t *temps);

  // Flush buffered samples to flash (call ~every few seconds).
  void flush();

  // Finalise: flush, patch record_count, close.
  bool endSession();

  bool isRecording() const { return recording_; }
  uint32_t recordCount() const { return recordCount_; }

  // Management.
  std::vector<SessionInfo> listSessions();
  bool deleteSession(const String &name);
  void enforceRetention(size_t maxSessions, size_t minFreeBytes);

 private:
  static constexpr int kBatchRecords = 32;
  static const char *kDir;

  bool recording_ = false;
  File file_;
  uint8_t cols_ = 0, rows_ = 0;
  size_t stride_ = 0;
  uint32_t recordCount_ = 0;

  std::vector<uint8_t> buf_;  // batched record bytes
  size_t buffered_ = 0;       // records currently in buf_

  bool flushBuffer();
};

}  // namespace tyre

#endif  // SRC_STORAGE_SESSIONLOGGER_H_
