#include "RotarySwitch.h"

#include "Config.h"

namespace luma {

void RotarySwitch::begin() {
  for (uint8_t i = 0; i < 8; ++i) {
    pinMode(pins::kRotary[i], INPUT_PULLUP);
  }
  // Resolve an initial position immediately if the knob is sitting on a detent.
  candidatePos_ = readActive();
  candidateSinceMs_ = millis();
  if (candidatePos_ >= 0) {
    stablePos_ = candidatePos_;
  }
  changed_ = false;
}

int RotarySwitch::readActive() const {
  int found = -1;
  for (uint8_t i = 0; i < 8; ++i) {
    if (digitalRead(pins::kRotary[i]) == LOW) {
      if (found >= 0) return -1; // more than one active -> transient
      found = i;
    }
  }
  return found; // -1 if none active
}

void RotarySwitch::update() {
  const int active = readActive();
  const uint32_t now = millis();

  // Only debounce valid single-position reads. Ignore zero/multi-active by
  // resetting the candidate window without disturbing the held stable value.
  if (active < 0) {
    candidatePos_ = -1;
    return;
  }

  if (active != candidatePos_) {
    candidatePos_ = active;
    candidateSinceMs_ = now;
    return;
  }

  if (candidatePos_ != stablePos_ &&
      (now - candidateSinceMs_) >= kRotaryStableMs) {
    stablePos_ = candidatePos_;
    changed_ = true;
  }
}

bool RotarySwitch::consumeChanged() {
  if (!changed_) return false;
  changed_ = false;
  return true;
}

} // namespace luma
