#include "LoadCap.h"

#include "Config.h"

namespace luma {

namespace {
constexpr char kPrefsNamespace[] = "luma";
constexpr char kAmpsKey[] = "maxA";
constexpr float kMinLoadAmps = 0.5f;

Preferences prefs_;
float amps_ = kDefaultMaxLoadAmps;
bool begun_ = false;

float clampAmps(float a) {
  if (!(a == a)) return 0.0f; // NaN
  if (a < kMinLoadAmps) return 0.0f;
  if (a > kDriverRatedAmps) a = kDriverRatedAmps;
  return a;
}

float dimVoltsFromAmps(float a) {
  return 10.0f * (a / kDriverRatedAmps);
}
} // namespace

void beginLoadCap() {
  if (begun_) return;
  prefs_.begin(kPrefsNamespace, false);
  if (prefs_.isKey(kAmpsKey)) {
    const float stored = prefs_.getFloat(kAmpsKey, kDefaultMaxLoadAmps);
    const float clamped = clampAmps(stored);
    amps_ = (clamped > 0.0f) ? clamped : kDefaultMaxLoadAmps;
  } else {
    amps_ = kDefaultMaxLoadAmps;
    prefs_.putFloat(kAmpsKey, amps_);
  }
  begun_ = true;
  Serial.printf("LoadCap: %.2f A (DIM cap %.2f V)\n", amps_, dimVoltsFromAmps(amps_));
}

float maxLoadAmps() { return amps_; }

float maxDimVolts() { return dimVoltsFromAmps(amps_); }

float maxOutputPercent() { return (amps_ / kDriverRatedAmps) * 100.0f; }

bool setMaxLoadAmps(float amps) {
  const float clamped = clampAmps(amps);
  if (clamped <= 0.0f) return false;
  amps_ = clamped;
  if (begun_) prefs_.putFloat(kAmpsKey, amps_);
  return true;
}

} // namespace luma
