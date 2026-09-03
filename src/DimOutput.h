#pragma once

#include <Arduino.h>

namespace luma {

// Owns the LEDC PWM channel that feeds the 0-10V converter board.
//
// Maps a user-facing level (0..100%) through the four-stage chain:
//   user% -> driver output% (clamped to kMaxOutputPercent) -> DIM volts ->
//   PWM duty (via the measured calibration LUT).
//
// Soft-ramps the driver output so relay make/break and setpoint changes never
// step the current instantly.
class DimOutput {
public:
  void begin();

  // Set the commanded user level (0..100). Internally clamped and mapped.
  void setUserLevel(float userPct);

  // Command the output down to the 10% floor (used while output is "off" or
  // during lockout, before the relay opens).
  void setToFloor();

  // Ramp toward the current target and write the PWM. Call every loop.
  void update();

  // Calibration hold: while enabled, update() stops writing so a raw duty set
  // via writeDutyFraction() stays put and isn't clobbered by the ramp loop.
  void setManualHold(bool on);
  bool isManualHold() const { return manualHold_; }

  // Direct duty write for the calibration sweep. Bypasses ramping/mapping.
  void writeDutyFraction(float duty01);

  float userLevel() const { return userLevel_; }          // clamped commanded user %
  float outputPercent() const { return currentOutPct_; }  // current ramped driver %
  float targetOutputPercent() const { return targetOutPct_; }
  bool atFloor() const;
  float estimatedAmps() const;

  // --- Pure mapping helpers (also used by the serial console) ---
  static float userLevelToOutputPercent(float userPct);
  static float outputPercentToVolts(float outPct);
  static float voltsToDuty01(float volts);

private:
  void applyOutputPercent(float outPct);

  float userLevel_ = 0.0f;
  float targetOutPct_ = 0.0f;   // driver output %, what we're ramping toward
  float currentOutPct_ = 0.0f;  // driver output %, current ramped value
  uint32_t lastRampMs_ = 0;
  bool begun_ = false;
  bool manualHold_ = false;
};

} // namespace luma
