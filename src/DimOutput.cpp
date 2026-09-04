#include "DimOutput.h"

#include "Config.h"
#include "LoadCap.h"

namespace luma {

void DimOutput::begin() {
  // ESP32 Arduino core 3.x LEDC API: attach frequency + resolution to the pin.
  ledcAttach(pins::kDimPwm, kDimPwmFreqHz, kDimPwmResBits);
  targetOutPct_ = kFloorOutputPercent;
  currentOutPct_ = kFloorOutputPercent;
  userLevel_ = 0.0f;
  lastRampMs_ = millis();
  applyOutputPercent(currentOutPct_);
  begun_ = true;
}

float DimOutput::userLevelToOutputPercent(float userPct) {
  if (userPct < 0.0f) userPct = 0.0f;
  if (userPct > 100.0f) userPct = 100.0f;
  return userPct;
}

float DimOutput::outputPercentToVolts(float userPct) {
  if (userPct < 0.0f) userPct = 0.0f;
  if (userPct > 100.0f) userPct = 100.0f;
  return kMinDimVolts + (userPct / 100.0f) * (maxDimVolts() - kMinDimVolts);
}

float DimOutput::voltsToDuty01(float volts) {
  if (volts <= kDimCalibration[0].volts) return kDimCalibration[0].duty01;
  const size_t last = kDimCalibrationCount - 1;
  if (volts >= kDimCalibration[last].volts) return kDimCalibration[last].duty01;

  for (size_t i = 1; i < kDimCalibrationCount; ++i) {
    const DimCalPoint &lo = kDimCalibration[i - 1];
    const DimCalPoint &hi = kDimCalibration[i];
    if (volts <= hi.volts) {
      const float t = (volts - lo.volts) / (hi.volts - lo.volts);
      return lo.duty01 + t * (hi.duty01 - lo.duty01);
    }
  }
  return kDimCalibration[last].duty01;
}

void DimOutput::setUserLevel(float userPct) {
  if (userPct < 0.0f) userPct = 0.0f;
  if (userPct > 100.0f) userPct = 100.0f;
  userLevel_ = userPct;
  targetOutPct_ = userLevelToOutputPercent(userPct);
}

void DimOutput::setToFloor() {
  targetOutPct_ = kFloorOutputPercent;
  // Leave userLevel_ untouched so the UI still shows the last commanded level
  // while the output is off.
}

void DimOutput::setSlewOverride(float pctPerSec) {
  slewOverride_ = (pctPerSec > 0.0f) ? pctPerSec : 0.0f;
}

void DimOutput::setManualHold(bool on) {
  manualHold_ = on;
  lastRampMs_ = millis();
}

void DimOutput::update() {
  if (manualHold_) return; // calibration owns the duty
  const uint32_t now = millis();
  float dt = (now - lastRampMs_) / 1000.0f;
  lastRampMs_ = now;
  if (dt <= 0.0f) return;
  if (dt > 0.25f) dt = 0.25f; // clamp after a long stall so we don't jump

  const float rate = (slewOverride_ > 0.0f) ? slewOverride_ : kRampRatePctPerSec;
  const float maxStep = rate * dt;
  const float delta = targetOutPct_ - currentOutPct_;
  if (fabsf(delta) <= maxStep) {
    currentOutPct_ = targetOutPct_;
  } else {
    currentOutPct_ += (delta > 0 ? maxStep : -maxStep);
  }
  applyOutputPercent(currentOutPct_);
}

void DimOutput::applyOutputPercent(float outPct) {
  if (outPct < 0.0f) outPct = 0.0f;
  if (outPct > 100.0f) outPct = 100.0f;
  const float volts = outputPercentToVolts(outPct);
  const float duty01 = voltsToDuty01(volts);
  writeDutyFraction(duty01);
}

void DimOutput::writeDutyFraction(float duty01) {
  if (duty01 < 0.0f) duty01 = 0.0f;
  if (duty01 > 1.0f) duty01 = 1.0f;
  const uint32_t duty = (uint32_t)lroundf(duty01 * kDimPwmMaxDuty);
  ledcWrite(pins::kDimPwm, duty);
}

bool DimOutput::atFloor() const {
  return currentOutPct_ <= 0.5f;
}

float DimOutput::dimVolts() const { return outputPercentToVolts(currentOutPct_); }

float DimOutput::estimatedAmps() const {
  // 100% user level is the NVS load cap, not the driver's 22 A rating.
  return (currentOutPct_ / 100.0f) * maxLoadAmps();
}

} // namespace luma
