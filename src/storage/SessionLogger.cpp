// Copyright (c) 2026 Sondre Sjølyst

#include "storage/SessionLogger.h"

#include <Preferences.h>
#include <ctype.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "helpers/PRINTHelper.h"
#include "processing/Downsample.h"

extern PRINTHelper printHelper;

namespace tyre {

const char *SessionLogger::kDir = "/sessions";

// Monotonic per-device counter, persisted across reboots. Filename prefix.
static uint32_t nextSeq() {
  Preferences p;
  p.begin("tyrelog", /*readOnly=*/false);
  uint32_t seq = p.getUInt("seq", 0);
  p.putUInt("seq", seq + 1);
  p.end();
  return seq;
}

// Filename-safe slug of the car name: alphanumerics only, max 16 chars.
static void carSlug(const char *car, char *out, size_t outSize) {
  size_t j = 0;
  for (size_t i = 0; car && car[i] && j + 1 < outSize && j < 16; i++) {
    char c = car[i];
    if (isalnum(static_cast<unsigned char>(c)))
      out[j++] = c;
  }
  if (j == 0) {
    strncpy(out, "car", outSize - 1);
    j = strlen(out);
  }
  out[j] = 0;
}

bool SessionLogger::begin() {
  if (!LittleFS.begin(/*formatOnFail=*/true)) {
    printHelper.log("ERROR", "LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(kDir)) {
    LittleFS.mkdir(kDir);
  }
  return true;
}

bool SessionLogger::startSession(uint32_t sessionId, uint64_t startEpochMs,
                                 uint8_t wheel, uint16_t rateHz,
                                 const uint8_t mac[6], const char *fwVer,
                                 uint32_t groupId, const char *carName,
                                 uint8_t optLo, uint8_t optHi) {
  if (recording_)
    return false;

  cols_ = kGridCols;
  rows_ = kGridRows;
  stride_ = recordStride(cols_, rows_);
  recordCount_ = 0;
  buffered_ = 0;
  buf_.assign(kBatchRecords * stride_, 0);

  enforceRetention(/*maxSessions=*/8, /*minFreeBytes=*/400 * 1024);

  char slug[20];
  carSlug(carName, slug, sizeof(slug));
  char path[72];
  snprintf(path, sizeof(path), "%s/%08u_%s_%s_%u.bin", kDir,
           static_cast<unsigned>(nextSeq()), slug, wheelName(wheel),
           static_cast<unsigned>(sessionId));
  file_ = LittleFS.open(path, "w");
  if (!file_) {
    printHelper.log("ERROR", "open session file failed: %s", path);
    return false;
  }

  LogHeader h{};
  h.magic = LOG_MAGIC;
  h.version = LOG_VERSION;
  h.grid_cols = cols_;
  h.grid_rows = rows_;
  h.sample_rate_hz = rateHz;
  h.wheel_pos = wheel;
  h.temp_scale = kTempScale;
  h.session_id = sessionId;
  h.session_start_epoch_ms = startEpochMs;
  h.group_id = groupId;
  if (mac)
    memcpy(h.device_mac, mac, 6);
  strncpy(h.fw_version, fwVer ? fwVer : "", sizeof(h.fw_version) - 1);
  strncpy(h.car_name, carName ? carName : "", sizeof(h.car_name) - 1);
  h.opt_lo = optLo;
  h.opt_hi = optHi;
  h.record_count = 0;

  if (file_.write(reinterpret_cast<uint8_t *>(&h), sizeof(h)) != sizeof(h)) {
    printHelper.log("ERROR", "write header failed");
    file_.close();
    return false;
  }
  file_.flush();
  recording_ = true;
  printHelper.log("INFO", "session started: %s", path);
  return true;
}

bool SessionLogger::appendSample(uint32_t tOffsetMs, const int16_t *temps) {
  if (!recording_)
    return false;

  uint8_t *p = buf_.data() + buffered_ * stride_;
  memcpy(p, &tOffsetMs, sizeof(uint32_t));
  memcpy(p + sizeof(uint32_t), temps, sizeof(int16_t) * cols_ * rows_);
  buffered_++;
  recordCount_++;

  if (buffered_ >= kBatchRecords) {
    return flushBuffer();
  }
  return true;
}

bool SessionLogger::flushBuffer() {
  if (buffered_ == 0)
    return true;
  const size_t bytes = buffered_ * stride_;
  bool ok = file_.write(buf_.data(), bytes) == bytes;
  if (!ok)
    printHelper.log("ERROR", "flush write failed");
  buffered_ = 0;
  return ok;
}

void SessionLogger::flush() {
  if (!recording_)
    return;
  flushBuffer();
  file_.flush();
}

bool SessionLogger::endSession() {
  if (!recording_)
    return false;
  flushBuffer();

  // Patch record_count in the header.
  file_.seek(offsetof(LogHeader, record_count), SeekSet);
  file_.write(reinterpret_cast<uint8_t *>(&recordCount_), sizeof(recordCount_));
  file_.flush();
  file_.close();
  recording_ = false;
  printHelper.log("INFO", "session ended: %u records",
                  static_cast<unsigned>(recordCount_));
  return true;
}

std::vector<SessionInfo> SessionLogger::listSessions() {
  std::vector<SessionInfo> out;
  File dir = LittleFS.open(kDir);
  if (!dir || !dir.isDirectory())
    return out;

  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory())
      continue;
    SessionInfo si;
    si.name = String(f.name());
    si.size = f.size();
    si.records = 0;
    si.session_id = 0;
    si.wheel = WHEEL_NONE;
    // Use the file's own header grid dims for the record count (files may have
    // been written under a different compile-time grid, e.g. uploaded wheels).
    LogHeader h{};
    // Read straight from the already-open dir entry (no second open).
    if (f.read(reinterpret_cast<uint8_t *>(&h), sizeof(h)) == sizeof(h) &&
        h.magic == LOG_MAGIC) {
      si.session_id = h.session_id;
      si.wheel = h.wheel_pos;
      size_t stride = recordStride(h.grid_cols, h.grid_rows);
      if (si.size > sizeof(LogHeader) && stride > 0)
        si.records = (si.size - sizeof(LogHeader)) / stride;
    }
    out.push_back(si);
  }
  return out;
}

bool SessionLogger::deleteSession(const String &name) {
  String path = String(kDir) + "/" + name;
  return LittleFS.remove(path);
}

void SessionLogger::enforceRetention(size_t maxSessions, size_t minFreeBytes) {
  size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
  std::vector<SessionInfo> sessions = listSessions();

  // Delete oldest first. Filenames carry a zero-padded monotonic sequence
  // prefix, so ascending name order == creation order (session_id is random).
  std::sort(sessions.begin(), sessions.end(),
            [](const SessionInfo &a, const SessionInfo &b) {
              return a.name < b.name;
            });

  size_t idx = 0;
  while ((sessions.size() - idx) > 0 &&
         ((sessions.size() - idx) >= maxSessions || freeBytes < minFreeBytes)) {
    if (deleteSession(sessions[idx].name)) {
      freeBytes += sessions[idx].size;
      printHelper.log("INFO", "retention: deleted %s",
                      sessions[idx].name.c_str());
    }
    idx++;
  }
}

}  // namespace tyre
