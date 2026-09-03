#include "ControlArbiter.h"

#include "Config.h"

namespace luma {

ControlArbiter::ControlArbiter(DimOutput &dim, RelayControl &relay,
                               RotarySwitch &rotary, KillSwitch &killSwitch,
                               Indicators &indicators)
    : dim_(dim),
      relay_(relay),
      rotary_(rotary),
      killSwitch_(killSwitch),
      indicators_(indicators) {}

const char *ControlArbiter::modeName(Mode m) {
  switch (m) {
    case Mode::Master: return "master";
    case Mode::Remote: return "remote";
    case Mode::Lockout: return "lockout";
  }
  return "?";
}

void ControlArbiter::begin() {
  lastKnobPos_ = rotary_.position();
  mode_ = Mode::Master;
  priorMode_ = Mode::Master;

  if (lastKnobPos_ >= 0) {
    setpoint_ = kKnobLevels[lastKnobPos_];
  } else {
    setpoint_ = 0.0f;
  }

  outputOn_ = kRestoreOutputOnBoot && (lastKnobPos_ >= 0);
  stage_ = OutStage::Off;
  stageSinceMs_ = millis();
  dim_.setToFloor();
}

void ControlArbiter::requestRemoteLevel(float pct) {
  portENTER_CRITICAL(&mux_);
  pendLevelFlag_ = true;
  pendLevel_ = pct;
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
        setpoint_ = kKnobLevels[pos];
        outputOn_ = true;
      }
    } else {
      outputOn_ = true; // remote resumes at its held setpoint
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
      setpoint_ = kKnobLevels[pos];
      outputOn_ = true;
    }
  }

  // --- Pending remote requests ---
  bool doRelease = false, doLevel = false, doOutput = false;
  float level = 0.0f;
  bool outputOn = false;
  portENTER_CRITICAL(&mux_);
  doRelease = pendReleaseFlag_;
  doLevel = pendLevelFlag_;
  level = pendLevel_;
  doOutput = pendOutputFlag_;
  outputOn = pendOutput_;
  pendReleaseFlag_ = pendLevelFlag_ = pendOutputFlag_ = false;
  portEXIT_CRITICAL(&mux_);

  if (doRelease) {
    mode_ = Mode::Master;
    const int pos = rotary_.position();
    if (pos >= 0) {
      setpoint_ = kKnobLevels[pos];
      lastKnobPos_ = pos;
    }
    outputOn_ = true;
  }
  if (doLevel) {
    mode_ = Mode::Remote;
    if (level < 0.0f) level = 0.0f;
    if (level > 100.0f) level = 100.0f;
    setpoint_ = level;
    outputOn_ = true;
  }
  if (doOutput) {
    // On/off works in either mode without changing which source owns level.
    outputOn_ = outputOn;
  }
}

void ControlArbiter::driveOutput() {
  const bool wantOn = outputOn_ && (mode_ != Mode::Lockout);
  const uint32_t now = millis();

  switch (stage_) {
    case OutStage::Off:
      dim_.setToFloor();
      if (wantOn && relay_.canClose()) {
        relay_.close();
        stage_ = OutStage::Closing;
        stageSinceMs_ = now;
      }
      break;

    case OutStage::Closing:
      // Hold at floor until contacts settle, then ramp to the setpoint.
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

    case OutStage::On:
      dim_.setUserLevel(setpoint_);
      if (!wantOn) {
        dim_.setToFloor(); // ramp down before breaking the relay
        stage_ = OutStage::Opening;
        stageSinceMs_ = now;
      }
      break;

    case OutStage::Opening:
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
  out.outputPercent = dim_.outputPercent();
  out.outputOn = outputOn_;
  out.relayClosed = relay_.isClosed();
  out.knobPosition = rotary_.position();
  out.lockout = (mode_ == Mode::Lockout);
  out.maxLevelPct = 100.0f;
  out.estimatedAmps = relay_.isClosed() ? dim_.estimatedAmps() : 0.0f;
}

} // namespace luma
