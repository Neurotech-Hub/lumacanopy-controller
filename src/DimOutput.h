#pragma once

#include <Arduino.h>

namespace luma {

// Owns the LEDC PWM channel that feeds the 0-10V converter board.
//
// Maps a user-facing level (0..100%) through:
//   user% -> DIM volts in [0, maxDimVolts()] (from NVS maxLoadAmps)
//         -> PWM duty via kDimCalibration
//
// Soft-ramps so relay make/break and setpoint changes never step instantly.
class DimOutput {
public:
  void begin();

  // Set the commanded user level (0..100). Internally clamped and mapped.
  void setUserLevel(float userPct);

  // Command DIM down to 0 V (used while output is "off" or during lockout,
  // before the relay opens).
  void setToFloor();

  // Ramp toward the current target and write the PWM. Call every loop.
  void update();

  // Ramp rate override for effect playback. 0 restores kRampRatePctPerSec.
  // Only OutStage::On sets this; relay sequencing keeps the gentle default.
  void setSlewOverride(float pctPerSec);

  // Calibration hold: while enabled, update() stops writing so a raw duty set
  // via writeDutyFraction() stays put and isn't clobbered by the ramp loop.
  void setManualHold(bool on);
  bool isManualHold() const { return manualHold_; }

  // Direct duty write for the calibration sweep. Bypasses ramping/mapping.
  void writeDutyFraction(float duty01);

  float userLevel() const { return userLevel_; }          // clamped commanded user %
  float outputPercent() const { return currentOutPct_; }  // ramped user % (0..100)
  float targetOutputPercent() const { return targetOutPct_; }
  float dimVolts() const;
  bool atFloor() const;
  float estimatedAmps() const;

  // --- Pure mapping helpers ---
  static float userLevelToOutputPercent(float userPct);
  static float outputPercentToVolts(float userPct);
  static float voltsToDuty01(float volts);

private:
  void applyOutputPercent(float outPct);

  float userLevel_ = 0.0f;
  float targetOutPct_ = 0.0f;   // ramped user %, what we're heading toward
  float currentOutPct_ = 0.0f;  // ramped user %, current value
  uint32_t lastRampMs_ = 0;
  float slewOverride_ = 0.0f;   // 0 = use kRampRatePctPerSec
  bool begun_ = false;
  bool manualHold_ = false;
};

} // namespace luma
