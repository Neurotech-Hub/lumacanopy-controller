#include "RelayControl.h"

#include "Config.h"

namespace luma {

void RelayControl::begin() {
  pinMode(pins::kRelay, OUTPUT);
  digitalWrite(pins::kRelay, LOW); // fail-safe: coil de-energized, output open
  closed_ = false;
  lastTransitionMs_ = millis();
  // Seed lastOpenMs_ far enough back that canClose() is honoured immediately but
  // the boot sequencing still respects a fresh settle.
  lastOpenMs_ = millis() - kMinRelayOffMs;
}

bool RelayControl::canClose() const {
  return (millis() - lastOpenMs_) >= kMinRelayOffMs;
}

void RelayControl::close() {
  if (closed_) return;
  digitalWrite(pins::kRelay, HIGH);
  closed_ = true;
  lastTransitionMs_ = millis();
}

void RelayControl::open() {
  if (!closed_) return;
  digitalWrite(pins::kRelay, LOW);
  closed_ = false;
  lastTransitionMs_ = millis();
  lastOpenMs_ = lastTransitionMs_;
}

} // namespace luma
