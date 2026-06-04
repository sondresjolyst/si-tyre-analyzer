// Copyright (c) 2026 Sondre Sjølyst
// One-button (GPIO0) multi-gesture handler with duration stages so the LED can
// give the user feedback about which gesture they are about to trigger:
//   tap (< PAIR_MS)            -> onTap   (start/stop recording)
//   hold >= PAIR_MS (& < AP)   -> onPair  (enter ESP-NOW pairing)
//   hold >= AP_MS              -> onAp    (enter AP/config mode)
// The gesture fires on release, based on the longest stage reached.

#ifndef SRC_HELPERS_BUTTONCONTROLLER_H_
#define SRC_HELPERS_BUTTONCONTROLLER_H_

#include <Arduino.h>

#include "config/DeviceConfig.h"

namespace tyre {

enum ButtonStage : uint8_t {
  STAGE_NONE = 0,  // not pressed
  STAGE_TAP = 1,   // pressed, below pairing threshold
  STAGE_PAIR = 2,  // crossed pairing threshold
  STAGE_AP = 3,    // crossed AP threshold
};

class ButtonController {
 public:
  typedef void (*Callback)();

  ButtonController(int pin, uint32_t pairMs = kPairHoldMs,
                   uint32_t apMs = kApHoldMs)
      : pin_(pin), pairMs_(pairMs), apMs_(apMs) {}

  void begin() { pinMode(pin_, INPUT_PULLUP); }

  void onTap(Callback cb) { onTap_ = cb; }
  void onPair(Callback cb) { onPair_ = cb; }
  void onAp(Callback cb) { onAp_ = cb; }

  // Current stage while held (for LED feedback); STAGE_NONE when released.
  ButtonStage stage() const { return stage_; }

  void update() {
    const bool pressed = (digitalRead(pin_) == LOW);
    const uint32_t now = millis();

    if (pressed && !wasPressed_) {
      pressStart_ = now;
      stage_ = STAGE_TAP;
    } else if (pressed && wasPressed_) {
      const uint32_t held = now - pressStart_;
      if (held >= apMs_)
        stage_ = STAGE_AP;
      else if (held >= pairMs_)
        stage_ = STAGE_PAIR;
      else
        stage_ = STAGE_TAP;
    } else if (!pressed && wasPressed_) {
      // Released — fire based on the stage reached.
      const ButtonStage fired = stage_;
      stage_ = STAGE_NONE;
      switch (fired) {
      case STAGE_AP:
        if (onAp_)
          onAp_();
        break;
      case STAGE_PAIR:
        if (onPair_)
          onPair_();
        break;
      case STAGE_TAP:
        if (onTap_)
          onTap_();
        break;
      default:
        break;
      }
    }
    wasPressed_ = pressed;
  }

 private:
  int pin_;
  uint32_t pairMs_;
  uint32_t apMs_;
  bool wasPressed_ = false;
  uint32_t pressStart_ = 0;
  ButtonStage stage_ = STAGE_NONE;
  Callback onTap_ = nullptr;
  Callback onPair_ = nullptr;
  Callback onAp_ = nullptr;
};

}  // namespace tyre

#endif  // SRC_HELPERS_BUTTONCONTROLLER_H_
