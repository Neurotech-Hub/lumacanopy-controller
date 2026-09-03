#pragma once

#include <Arduino.h>

namespace luma {

// Two case-mounted indicator LEDs.
//
// Status LED (GPIO13, digital): solid when the output is commanded on, off when
// open. Fast blink during kill-switch lockout -- which still never reads as a
// steady "on", preserving it as an unambiguous output-state indicator.
//
// Level LED (GPIO17, LEDC): gamma-corrected brightness tracking the setpoint.
// Steady == knob control; slow breathe (between ~40% and 100% of that
// brightness) == remote control, signalling the knob position is stale.
class Indicators {
public:
  void begin();
  void update(bool outputOn, bool lockout, float levelPct, bool remoteMode);

private:
  static uint8_t gamma8(float brightness01);
  bool begun_ = false;
};

} // namespace luma
