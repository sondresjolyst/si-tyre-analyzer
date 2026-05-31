// Copyright (c) 2026 Sondre Sjølyst
// Compile-time defaults (from build flags) + the runtime device config struct.
// Pure C++ — no Arduino headers — so it can be included by native unit tests.

#ifndef SRC_CONFIG_DEVICECONFIG_H_
#define SRC_CONFIG_DEVICECONFIG_H_

#include <stdint.h>
#include <string.h>

// ── Build-flag defaults ──────────────────────────────────────────────────────
#ifndef GRID_COLS
#define GRID_COLS 6
#endif
#ifndef GRID_ROWS
#define GRID_ROWS 3
#endif
#ifndef SAMPLE_RATE_HZ
#define SAMPLE_RATE_HZ 4
#endif
#ifndef SESSION_MAX_SEC
#define SESSION_MAX_SEC 1800
#endif
#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 1
#endif
#ifndef HAS_SENSOR
#define HAS_SENSOR 1
#endif
#ifndef WHEEL_POS
#define WHEEL_POS "FL"
#endif
#ifndef DEVICE_ROLE
#define DEVICE_ROLE "master"
#endif

namespace tyre {

constexpr int kGridCols = GRID_COLS;
constexpr int kGridRows = GRID_ROWS;
constexpr int kGridCells = kGridCols * kGridRows;
constexpr int kMaxPeers = 4;

enum WheelPos : uint8_t {
  WHEEL_FL = 0,
  WHEEL_FR = 1,
  WHEEL_RL = 2,
  WHEEL_RR = 3,
  WHEEL_NONE = 255,
};

enum Role : uint8_t {
  ROLE_SLAVE = 0,
  ROLE_MASTER = 1,
};

inline const char *wheelName(uint8_t w) {
  switch (w) {
  case WHEEL_FL:
    return "FL";
  case WHEEL_FR:
    return "FR";
  case WHEEL_RL:
    return "RL";
  case WHEEL_RR:
    return "RR";
  default:
    return "NA";
  }
}

inline uint8_t wheelFromName(const char *s) {
  if (!s)
    return WHEEL_NONE;
  if (!strcmp(s, "FL"))
    return WHEEL_FL;
  if (!strcmp(s, "FR"))
    return WHEEL_FR;
  if (!strcmp(s, "RL"))
    return WHEEL_RL;
  if (!strcmp(s, "RR"))
    return WHEEL_RR;
  return WHEEL_NONE;
}

inline uint8_t roleFromName(const char *s) {
  return (s && !strcmp(s, "master")) ? ROLE_MASTER : ROLE_SLAVE;
}

// One paired peer (master's view of a wheel unit).
struct PeerEntry {
  uint8_t mac[6];
  uint8_t wheel;   // WheelPos
  uint8_t in_use;  // 0/1
};

// Persisted device configuration (NVS via Settings).
struct DeviceConfig {
  uint8_t role;         // Role
  uint8_t wheel;        // WheelPos (this device's wheel; WHEEL_NONE for master)
  uint8_t has_sensor;   // 1 wheel unit, 0 dash master
  uint8_t has_display;  // 1 if an ILI9341 dash gauge is fitted (master only)
  uint8_t channel;      // ESP-NOW / SoftAP channel (1/6/11)
  uint32_t group_id;    // per-car isolation tag
  char car_name[24];    // user car label (e.g. "Volvo 242 Turbo"); "" if unset

  // Slave: the master it is paired with.
  uint8_t master_mac[6];
  uint8_t has_master;    // 0/1
  char master_ssid[33];  // master SoftAP name, for post-session upload

  // Master: paired wheel units.
  PeerEntry peers[kMaxPeers];

  void setDefaults() {
    memset(this, 0, sizeof(*this));
    role = roleFromName(DEVICE_ROLE);
    wheel = (HAS_SENSOR) ? wheelFromName(WHEEL_POS) : WHEEL_NONE;
    has_sensor = HAS_SENSOR ? 1 : 0;
    channel = ESPNOW_CHANNEL;
    group_id = 0;  // set at first pairing
  }
};

}  // namespace tyre

#endif  // SRC_CONFIG_DEVICECONFIG_H_
