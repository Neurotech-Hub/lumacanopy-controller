#pragma once

#include <Arduino.h>

namespace luma {

// Drives the output-side relay (normally open, active-high coil).
//
// Enforces an anti-cycling minimum off-time. The actual close/open *sequencing*
// against the dimmer (settle before ramp up, ramp down before open) lives in
// ControlArbiter; this class just owns the coil and the timing guard.
class RelayControl {
public:
  void begin();

  // True once the min off-time has elapsed since the last open.
  bool canClose() const;

  void close();
  void open();

  bool isClosed() const { return closed_; }

  // Milliseconds since the relay last transitioned into its current state.
  uint32_t timeInStateMs() const { return millis() - lastTransitionMs_; }

private:
  bool closed_ = false;
  uint32_t lastTransitionMs_ = 0;
  uint32_t lastOpenMs_ = 0;
};

} // namespace luma
