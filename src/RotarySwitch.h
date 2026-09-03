#pragma once

#include <Arduino.h>

namespace luma {

// 8-position rotary switch on INPUT_PULLUP pins, common to GND.
//
// A position is "active" when its pin reads LOW. Real switches make-before-break
// or break-before-make during rotation, so we only accept a candidate when
// exactly one pin is active AND it has held steady for kRotaryStableMs. Zero-
// active (break-before-make gap) and multi-active (make-before-break overlap)
// transients are ignored, and the last stable position is held throughout.
class RotarySwitch {
public:
  void begin();
  void update();

  // Last resolved stable position, 0..7. -1 only before the first resolve
  // (e.g. knob physically between detents at boot).
  int position() const { return stablePos_; }

  // Returns true exactly once after the stable position changes. Used by the
  // arbiter to detect user intent (knob movement), not absolute value.
  bool consumeChanged();

private:
  int readActive() const; // exactly-one-active -> 0..7, else -1

  int stablePos_ = -1;      // last accepted stable position
  int candidatePos_ = -1;   // position currently being debounced
  uint32_t candidateSinceMs_ = 0;
  bool changed_ = false;
};

} // namespace luma
