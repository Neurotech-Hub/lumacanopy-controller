#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include "DimOutput.h"
#include "EffectEngine.h"
#include "Indicators.h"
#include "KillSwitch.h"
#include "RelayControl.h"
#include "RotarySwitch.h"
#include "SlotStore.h"

namespace luma {

// The brain. Owns the Master/Remote/Lockout state machine, arbitrates between
// the knob and remote (serial/Wi-Fi) commands, and sequences the relay against
// the dimmer.
//
// Control rule: knob *movement* (not value) reclaims Master. Remote holds its
// last commanded level indefinitely if the client disappears -- no timeout.
//
// What runs at any moment is a *program* (SlotConfig), not a bare level:
//  - Master: the slot for the current knob position.
//  - Remote: either a slot selected remotely (activeSlot 0..7) or a free level
//    from the slider (activeSlot -1, held in freeSlot_ as a synthetic Steady).
// The active slot is re-read from SlotStore every update, so editing the live
// slot from the web UI takes effect immediately.
//
// Request setters are called from the async web-server task; they only stash
// intent under a spinlock and are applied in update() on the main loop.
class ControlArbiter {
public:
  enum class Mode : uint8_t { Master, Remote, Lockout };

  struct State {
    Mode mode = Mode::Master;
    Mode priorMode = Mode::Master;
    float setpointPct = 0.0f;    // nominal user level 0..100 (effect peak)
    float instantPct = 0.0f;     // instantaneous level the effect is commanding
    float outputPercent = 0.0f;  // current ramped user level %
    bool outputOn = false;       // relay commanded closed
    bool relayClosed = false;    // relay actually closed right now
    int knobPosition = -1;       // 0..7
    int activeSlot = -1;         // 0..7, or -1 for a free remote level
    EffectKind effect = EffectKind::Steady;
    bool lockout = false;
    float maxLevelPct = 100.0f;
    float estimatedAmps = 0.0f;
    // Wi-Fi status (filled by the sketch before broadcast).
    bool wifiConnected = false;
    String ip;
    String wifiMode;
  };

  ControlArbiter(DimOutput &dim, RelayControl &relay, RotarySwitch &rotary,
                 KillSwitch &killSwitch, Indicators &indicators,
                 SlotStore &slots);

  void begin();
  void update();

  // --- Remote requests (thread-safe) ---
  void requestRemoteLevel(float pct); // enter Remote, free level, output on
  void requestSlot(int index);        // enter Remote running slot 0..7
  void requestOutput(bool on);        // relay on/off in the current mode
  void requestRelease();              // hand control back to the knob

  void snapshot(State &out) const;

  static const char *modeName(Mode m);

private:
  enum class OutStage : uint8_t { Off, Closing, On, Opening };

  void applyModeAndSetpoint();
  void driveOutput();

  // The program currently running: the live slot, or the synthetic free-level
  // slot when a slider level owns the output.
  SlotConfig activeConfig() const;

  DimOutput &dim_;
  RelayControl &relay_;
  RotarySwitch &rotary_;
  KillSwitch &killSwitch_;
  Indicators &indicators_;
  SlotStore &slots_;

  Mode mode_ = Mode::Master;
  Mode priorMode_ = Mode::Master;
  float setpoint_ = 0.0f;
  float instant_ = 0.0f;
  bool outputOn_ = false;
  int lastKnobPos_ = -1;
  int activeSlotIndex_ = -1;   // -1 = free level in freeSlot_
  SlotConfig freeSlot_;        // synthetic Steady slot for slider control
  OutStage stage_ = OutStage::Off;
  uint32_t stageSinceMs_ = 0;

  // Pending remote requests, guarded by mux_.
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  bool pendReleaseFlag_ = false;
  bool pendLevelFlag_ = false;
  float pendLevel_ = 0.0f;
  bool pendSlotFlag_ = false;
  int pendSlot_ = -1;
  bool pendOutputFlag_ = false;
  bool pendOutput_ = false;
};

} // namespace luma
