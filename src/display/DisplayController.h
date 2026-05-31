// Copyright (c) 2026 Sondre Sjølyst

#ifndef SRC_DISPLAY_DISPLAYCONTROLLER_H_
#define SRC_DISPLAY_DISPLAYCONTROLLER_H_

#if HAS_DISPLAY

#include <stdint.h>

namespace tyre {

class LiveDashboard;

class DisplayController {
 public:
  bool begin();
  void render(const LiveDashboard &dash);
  bool getTouch(uint16_t *x, uint16_t *y);

 private:
  void drawCell(int slot, const char *label, const int16_t *temps, bool valid);

  bool ready_ = false;
  uint32_t lastDrawMs_ = 0;
};

}  // namespace tyre

#endif  // HAS_DISPLAY
#endif  // SRC_DISPLAY_DISPLAYCONTROLLER_H_
