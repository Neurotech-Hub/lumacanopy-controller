#include "KillSwitch.h"

#include "Config.h"

namespace luma {

namespace {
constexpr uint32_t kDebounceMs = 30;
}

void KillSwitch::begin() {
  pinMode(pins::kKillSwitch, INPUT_PULLUP);
  // Active-low: LOW == asserted (supply off).
  const bool raw = (digitalRead(pins::kKillSwitch) == LOW);
  lastRaw_ = raw;
  lockout_ = raw;
  lastChangeMs_ = millis();
}

void KillSwitch::update() {
  const bool raw = (digitalRead(pins::kKillSwitch) == LOW);
  const uint32_t now = millis();

  if (raw != lastRaw_) {
    lastRaw_ = raw;
    lastChangeMs_ = now;
    return;
  }

  if (raw != lockout_ && (now - lastChangeMs_) >= kDebounceMs) {
    lockout_ = raw;
  }
}

} // namespace luma
