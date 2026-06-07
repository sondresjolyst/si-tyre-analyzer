// Copyright (c) 2026 Sondre Sjølyst
// On-flash binary log format. Pure header (no Arduino) — native-test safe.
// Little-endian; ESP32 and x86 are both LE. The Python viewer parses the same
// layout. Keep this struct and tools/tyreviz/logreader.py in lockstep.

#ifndef SRC_STORAGE_LOGFORMAT_H_
#define SRC_STORAGE_LOGFORMAT_H_

#include <stddef.h>
#include <stdint.h>

namespace tyre {

static const uint32_t LOG_MAGIC = 0x54595254;  // "TYRT" little-endian
static const uint16_t LOG_VERSION = 2;

// LogHeader.flags bits (v2+).
static const uint8_t LOG_FLAG_FLIP_X = 1 << 0;  // columns mirrored at capture
static const uint8_t LOG_FLAG_FLIP_Y = 1 << 1;  // rows mirrored at capture
static const uint8_t LOG_FLAG_MOCK = 1 << 2;    // recorded from the mock sensor

#pragma pack(push, 1)
struct LogHeader {                  // exactly 96 bytes
  uint32_t magic;                   // LOG_MAGIC
  uint16_t version;                 // LOG_VERSION
  uint8_t grid_cols;                // e.g. 6
  uint8_t grid_rows;                // e.g. 3
  uint16_t sample_rate_hz;          // configured sample rate
  uint8_t wheel_pos;                // WheelPos enum (0=FL 1=FR 2=RL 3=RR)
  uint8_t temp_scale;               // stored = degC * temp_scale (100)
  uint32_t session_id;              // master-chosen, incremental; groups wheels
  uint64_t session_start_epoch_ms;  // 0 if wall-clock unknown (no NTP)
  uint8_t device_mac[6];            // this device's efuse MAC
  uint32_t group_id;                // per-car isolation tag
  char fw_version[16];              // "v0.1.0", null-padded
  uint32_t record_count;            // patched at close; 0 if not finalised
  char car_name[24];                // user car label, null-padded ("" if unset)
  uint8_t opt_lo;                   // optimal window low edge (degC); v2+
  uint8_t opt_hi;                   // optimal window high edge (degC); v2+
  uint8_t flags;                    // LOG_FLAG_* bitfield; v2+
  uint8_t reserved[15];             // pad to 96
};

struct SampleRecord {    // fixed size = 4 + 2*cols*rows
  uint32_t t_offset_ms;  // ms since session_start
  int16_t temps[];       // grid_cols*grid_rows, row-major, degC*100
};
#pragma pack(pop)

static_assert(sizeof(LogHeader) == 96, "LogHeader must be 96 bytes");

// Byte size of one sample record for the given grid.
inline size_t recordStride(int cols, int rows) {
  return sizeof(uint32_t) + sizeof(int16_t) * cols * rows;
}

}  // namespace tyre

#endif  // SRC_STORAGE_LOGFORMAT_H_
