// Copyright (c) 2026 Sondre Sjølyst
// Master-side store of the latest live grid per wheel, plus the HTTP routes
// that serve the 2x2 dashboard over the master SoftAP.

#ifndef SRC_CONTROLLERS_LIVEDASHBOARD_H_
#define SRC_CONTROLLERS_LIVEDASHBOARD_H_

#include <stdint.h>

#include "config/DeviceConfig.h"

namespace tyre {

class LiveDashboard {
 public:
  void update(uint8_t wheel, uint32_t tOffsetMs, const int16_t *temps);

  struct WheelLive {
    int16_t temps[kGridCells];
    uint32_t t_offset_ms;
    uint32_t last_seen_ms;
    bool valid;
  };

  // Atomically copy wheel i into *dst (safe across the recv-callback/web-task
  // boundary). Returns dst->valid.
  bool snapshot(int i, WheelLive *dst) const;

 private:
  WheelLive wheels_[4] = {};
};

// Attach /live (HTML) + /api/live (JSON) to the global server.
void registerLiveRoutes();

}  // namespace tyre

#endif  // SRC_CONTROLLERS_LIVEDASHBOARD_H_
