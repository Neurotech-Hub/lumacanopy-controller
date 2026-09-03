#pragma once

#include <Arduino.h>

namespace luma {

// Maintained panel ON/OFF switch: GPIO with INPUT_PULLUP, other side to GND.
// With kKillSwitchClosedMeansOn, closed (LOW) is labeled ON (output allowed)
// and open (HIGH) is labeled OFF (lockout).
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
