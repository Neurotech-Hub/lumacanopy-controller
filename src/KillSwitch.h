#pragma once

#include <Arduino.h>

namespace luma {

// Maintained external kill switch on an INPUT_PULLUP pin, active-low.
//
// While asserted, the arbiter hard-locks the output off; nothing (knob or
// Wi-Fi) can override it. Debounced so contact bounce doesn't flap the lockout.
class KillSwitch {
public:
  void begin();
  void update();

  // True while the switch is flipped to the "supply off" position.
  bool isLockout() const { return lockout_; }

private:
  bool lockout_ = false;
  bool lastRaw_ = false;
  uint32_t lastChangeMs_ = 0;
};

} // namespace luma
