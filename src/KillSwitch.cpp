#include "KillSwitch.h"

#include "Config.h"

namespace luma {

namespace {
constexpr uint32_t kDebounceMs = 30;

// Closed switch pulls the pin to GND (LOW). Open leaves the pull-up (HIGH).
bool readLockout() {
  const bool closed = (digitalRead(pins::kKillSwitch) == LOW);
  return kKillSwitchClosedMeansOn ? !closed : closed;
}
} // namespace

void KillSwitch::begin() {
  pinMode(pins::kKillSwitch, INPUT_PULLUP);
  const bool raw = readLockout();
  lastRaw_ = raw;
  lockout_ = raw;
  lastChangeMs_ = millis();
}

void KillSwitch::update() {
  const bool raw = readLockout();
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
