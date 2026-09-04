#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace luma {

// What a knob position does. A slot is a *program*, not just a level.
//
// Blink/Breathe run entirely in the DIM domain -- the relay never cycles for an
// effect. Off commands the output open at that detent.
enum class EffectKind : uint8_t {
  Steady = 0,
  Blink = 1,
  Breathe = 2,
  Off = 3,
};

// One knob position's program. Steady uses highPct only; Blink and Breathe
// swing between lowPct and highPct over onMs/offMs.
struct SlotConfig {
  EffectKind kind = EffectKind::Steady;
  float highPct = 0.0f;
  float lowPct = 0.0f;
  uint16_t onMs = 500;
  uint16_t offMs = 500;
};

const char *effectName(EffectKind k);
bool effectFromName(const String &name, EffectKind &out);

// Clamps levels to 0..100 (low <= high) and effect timing to the rate cap in
// Config.h. Always applied before a slot is stored, so nothing downstream has
// to re-validate.
void clampSlot(SlotConfig &s);

// The eight knob programs, persisted in NVS.
//
// Held in RAM and served from there, so the arbiter can re-read the live slot
// every loop and pick up edits immediately. NVS is written only on set().
// get()/set() are spinlock-guarded because the web-server task writes while the
// main loop reads.
class SlotStore {
public:
  static constexpr int kCount = 8;

  void begin(); // load from NVS; seed factory defaults on first boot

  SlotConfig get(int index) const;              // index 0..7, clamped
  void set(int index, const SlotConfig &cfg);   // clamps, stores, persists
  void resetDefaults();

  // Factory default for a position: Steady at kKnobLevels[index], i.e. exactly
  // the pre-programmable behaviour.
  static SlotConfig factoryDefault(int index);

private:
  void save();

  SlotConfig slots_[kCount];
  Preferences prefs_;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};

} // namespace luma
