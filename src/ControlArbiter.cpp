#include "ControlArbiter.h"

#include "Config.h"

namespace luma {

ControlArbiter::ControlArbiter(DimOutput &dim, RelayControl &relay,
                               RotarySwitch &rotary, KillSwitch &killSwitch,
                               Indicators &indicators, SlotStore &slots)
    : dim_(dim),
      relay_(relay),
      rotary_(rotary),
      killSwitch_(killSwitch),
      indicators_(indicators),
      slots_(slots) {}

const char *ControlArbiter::modeName(Mode m) {
  switch (m) {
    case Mode::Master: return "master";
    case Mode::Remote: return "remote";
    case Mode::Lockout: return "lockout";
  }
  return "?";
}

SlotConfig ControlArbiter::activeConfig() const {
  if (activeSlotIndex_ >= 0) return slots_.get(activeSlotIndex_);
  return freeSlot_;
}

void ControlArbiter::begin() {
  lastKnobPos_ = rotary_.position();
  mode_ = Mode::Master;
  priorMode_ = Mode::Master;

  freeSlot_ = SlotConfig{}; // Steady at 0%
  activeSlotIndex_ = lastKnobPos_; // -1 if the knob is between detents at boot
  setpoint_ = nominalLevel(activeConfig());

  outputOn_ = kRestoreOutputOnBoot && (lastKnobPos_ >= 0);
  instant_ = 0.0f;
  stage_ = OutStage::Off;
  stageSinceMs_ = millis();
  dim_.setSlewOverride(0.0f);
  dim_.setToFloor();
}

void ControlArbiter::requestRemoteLevel(float pct) {
  portENTER_CRITICAL(&mux_);
  pendLevelFlag_ = true;
  pendLevel_ = pct;
  portEXIT_CRITICAL(&mux_);
}

void ControlArbiter::requestSlot(int index) {
  portENTER_CRITICAL(&mux_);
  pendSlotFlag_ = true;
  pendSlot_ = index;
  portEXIT_CRITICAL(&mux_);
}

void ControlArbiter::requestOutput(bool on) {
  portENTER_CRITICAL(&mux_);
  pendOutputFlag_ = true;
  pendOutput_ = on;
  portEXIT_CRITICAL(&mux_);
}

void ControlArbiter::requestRelease() {
  portENTER_CRITICAL(&mux_);
  pendReleaseFlag_ = true;
  portEXIT_CRITICAL(&mux_);
}

void ControlArbiter::update() {
  rotary_.update();
  killSwitch_.update();

  applyModeAndSetpoint();
  driveOutput();

  dim_.update();

  const bool remoteMode = (mode_ == Mode::Remote);
  indicators_.update(outputOn_ && relay_.isClosed(), mode_ == Mode::Lockout,
                     setpoint_, remoteMode);
}

void ControlArbiter::applyModeAndSetpoint() {
  const bool lockout = killSwitch_.isLockout();

  // --- Lockout takes absolute priority ---
  if (lockout) {
    if (mode_ != Mode::Lockout) {
      priorMode_ = mode_;
      mode_ = Mode::Lockout;
    }
    outputOn_ = false;
    // Consume the knob-changed edge so a knob turn during lockout doesn't
    // spuriously fire the moment we release.
    rotary_.consumeChanged();
    lastKnobPos_ = rotary_.position();
    return;
  }

  // --- Just released from lockout: restore the prior mode ---
  if (mode_ == Mode::Lockout) {
    mode_ = priorMode_;
    // Re-arm the output if the restored mode had it on.
    // (setpoint_ and outputOn_ from before lockout are preserved except
    // outputOn_ which we cleared; restore based on mode intent below.)
    if (mode_ == Mode::Master) {
      const int pos = rotary_.position();
      if (pos >= 0) {
        activeSlotIndex_ = pos;
        outputOn_ = true;
      }
    } else {
      outputOn_ = true; // remote resumes at its held program
    }
    rotary_.consumeChanged();
    lastKnobPos_ = rotary_.position();
  }

  // --- Knob movement reclaims Master (detect movement, not value) ---
  if (rotary_.consumeChanged()) {
    const int pos = rotary_.position();
    if (pos >= 0) {
      lastKnobPos_ = pos;
      mode_ = Mode::Master;
      activeSlotIndex_ = pos;
      outputOn_ = true;
    }
  }

  // --- Pending remote requests ---
  bool doRelease = false, doLevel = false, doSlot = false, doOutput = false;
  float level = 0.0f;
  int slot = -1;
  bool outputOn = false;
  portENTER_CRITICAL(&mux_);
  doRelease = pendReleaseFlag_;
  doLevel = pendLevelFlag_;
  level = pendLevel_;
  doSlot = pendSlotFlag_;
  slot = pendSlot_;
  doOutput = pendOutputFlag_;
  outputOn = pendOutput_;
  pendReleaseFlag_ = pendLevelFlag_ = pendSlotFlag_ = pendOutputFlag_ = false;
  portEXIT_CRITICAL(&mux_);

  if (doRelease) {
    mode_ = Mode::Master;
    const int pos = rotary_.position();
    if (pos >= 0) {
      activeSlotIndex_ = pos;
      lastKnobPos_ = pos;
    }
    outputOn_ = true;
  }
  if (doSlot && slot >= 0 && slot < SlotStore::kCount) {
    // Remotely running one of the knob's programs. The physical knob keeps its
    // position; turning it still reclaims Master.
    mode_ = Mode::Remote;
    activeSlotIndex_ = slot;
    outputOn_ = true;
  }
  if (doLevel) {
    // Free level from the slider: a synthetic Steady program, so everything
    // downstream sees one uniform "run this program" path.
    mode_ = Mode::Remote;
    if (level < 0.0f) level = 0.0f;
    if (level > 100.0f) level = 100.0f;
    freeSlot_ = SlotConfig{};
    freeSlot_.kind = EffectKind::Steady;
    freeSlot_.highPct = level;
    activeSlotIndex_ = -1;
    outputOn_ = true;
  }
  if (doOutput) {
    // On/off works in either mode without changing which source owns level.
    outputOn_ = outputOn;
  }

  // --- Derive the nominal level from whatever program is now active ---
  // Re-read every pass so a live edit to the running slot applies at once.
  const SlotConfig cfg = activeConfig();
  setpoint_ = nominalLevel(cfg);
  if (cfg.kind == EffectKind::Off) {
    // A position programmed Off commands the output open. The ON button can't
    // override it -- pick another position or level first.
    outputOn_ = false;
  }
}

void ControlArbiter::driveOutput() {
  const bool wantOn = outputOn_ && (mode_ != Mode::Lockout);
  const uint32_t now = millis();

  switch (stage_) {
    case OutStage::Off:
      dim_.setSlewOverride(0.0f);
      instant_ = 0.0f;
      dim_.setToFloor();
      if (wantOn && relay_.canClose()) {
        relay_.close();
        stage_ = OutStage::Closing;
        stageSinceMs_ = now;
      }
      break;

    case OutStage::Closing:
      // Hold at floor until contacts settle, then ramp to the setpoint. The
      // gentle default ramp owns this stage; effects only run once we are On.
      dim_.setSlewOverride(0.0f);
      instant_ = 0.0f;
      dim_.setToFloor();
      if (!wantOn) {
        stage_ = OutStage::Opening;
        stageSinceMs_ = now;
      } else if (now - stageSinceMs_ >= kRelaySettleMs) {
        dim_.setUserLevel(setpoint_);
        stage_ = OutStage::On;
        stageSinceMs_ = now;
      }
      break;

    case OutStage::On: {
      // The effect runs purely in the DIM domain -- the relay stays closed for
      // the whole program. Never sequence the relay from here.
      const SlotConfig cfg = activeConfig();
      float slew = 0.0f;
      instant_ = effectLevel(cfg, now, slew);
      dim_.setSlewOverride(slew);
      dim_.setUserLevel(instant_);
      if (!wantOn) {
        dim_.setSlewOverride(0.0f);
        dim_.setToFloor(); // ramp down before breaking the relay
        stage_ = OutStage::Opening;
        stageSinceMs_ = now;
      }
      break;
    }

    case OutStage::Opening:
      dim_.setSlewOverride(0.0f);
      instant_ = 0.0f;
      dim_.setToFloor();
      // Wait for the ramp to reach the floor (or a safety timeout) before
      // opening, so the relay never breaks near full current.
      if (dim_.atFloor() || (now - stageSinceMs_ >= 1500)) {
        relay_.open();
        stage_ = OutStage::Off;
        stageSinceMs_ = now;
      }
      break;
  }
}

void ControlArbiter::snapshot(State &out) const {
  out.mode = mode_;
  out.priorMode = priorMode_;
  out.setpointPct = setpoint_;
  out.instantPct = instant_;
  out.outputPercent = dim_.outputPercent();
  out.outputOn = outputOn_;
  out.relayClosed = relay_.isClosed();
  out.knobPosition = rotary_.position();
  out.activeSlot = activeSlotIndex_;
  out.effect = activeConfig().kind;
  out.lockout = (mode_ == Mode::Lockout);
  out.maxLevelPct = 100.0f;
  out.estimatedAmps = relay_.isClosed() ? dim_.estimatedAmps() : 0.0f;
}

} // namespace luma
