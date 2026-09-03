#include "Indicators.h"

#include <math.h>

#include "Config.h"

namespace luma {

namespace {
constexpr uint32_t kLockoutBlinkMs = 150; // fast blink half-period
constexpr float kBreathePeriodMs = 2500.0f;
constexpr float kBreatheFloor = 0.40f; // remote breathe dips to 40% of level brightness
} // namespace

void Indicators::begin() {
  pinMode(pins::kStatusLed, OUTPUT);
  digitalWrite(pins::kStatusLed, LOW);
  ledcAttach(pins::kLevelLed, kLevelLedFreqHz, kLevelLedResBits);
  ledcWrite(pins::kLevelLed, 0);
  begun_ = true;
}

uint8_t Indicators::gamma8(float brightness01) {
  if (brightness01 < 0.0f) brightness01 = 0.0f;
  if (brightness01 > 1.0f) brightness01 = 1.0f;
  // Gamma 2.2 so perceived brightness tracks the level roughly linearly.
  const float corrected = powf(brightness01, 2.2f);
  return (uint8_t)lroundf(corrected * kLevelLedMaxDuty);
}

void Indicators::update(bool outputOn, bool lockout, float levelPct,
                        bool remoteMode) {
  const uint32_t now = millis();

  // --- Status LED ---
  if (lockout) {
    const bool on = ((now / kLockoutBlinkMs) & 1u) != 0;
    digitalWrite(pins::kStatusLed, on ? HIGH : LOW);
  } else {
    digitalWrite(pins::kStatusLed, outputOn ? HIGH : LOW);
  }

  // --- Level LED ---
  float brightness = levelPct / 100.0f;
  if (!outputOn || lockout) {
    brightness = 0.0f;
  } else if (remoteMode) {
    // Breathe between kBreatheFloor and 1.0 of the level brightness.
    const float phase = (now / kBreathePeriodMs);
    const float wave = 0.5f * (1.0f + sinf(phase * 2.0f * 3.14159265f));
    const float scale = kBreatheFloor + (1.0f - kBreatheFloor) * wave;
    brightness *= scale;
  }
  ledcWrite(pins::kLevelLed, gamma8(brightness));
}

} // namespace luma
