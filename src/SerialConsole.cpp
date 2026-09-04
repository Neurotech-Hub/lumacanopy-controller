#include "SerialConsole.h"

#include "Config.h"
#include "LoadCap.h"

namespace luma {

SerialConsole::SerialConsole(ControlArbiter &arbiter, DimOutput &dim)
    : arbiter_(arbiter), dim_(dim) {}

void SerialConsole::begin() { buf_.reserve(64); }

void SerialConsole::update() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String line = buf_;
      buf_ = "";
      line.trim();
      if (line.length() > 0) handleLine(line);
    } else {
      buf_ += c;
      if (buf_.length() > 120) buf_ = ""; // runaway guard
    }
  }
}

void SerialConsole::handleLine(const String &line) {
  int sp = line.indexOf(' ');
  String cmd = (sp < 0) ? line : line.substring(0, sp);
  String args = (sp < 0) ? "" : line.substring(sp + 1);
  args.trim();
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "status" || cmd == "s") {
    printStatus();
  } else if (cmd == "level" || cmd == "l") {
    const float pct = args.toFloat();
    arbiter_.requestRemoteLevel(pct);
    Serial.printf("OK remote level -> %.1f%%\n", pct);
  } else if (cmd == "on") {
    arbiter_.requestOutput(true);
    Serial.println(F("OK output on"));
  } else if (cmd == "off") {
    arbiter_.requestOutput(false);
    Serial.println(F("OK output off"));
  } else if (cmd == "release") {
    arbiter_.requestRelease();
    Serial.println(F("OK released to knob"));
  } else if (cmd == "maxamps" || cmd == "amps") {
    if (args.length() == 0) {
      Serial.printf("maxLoadAmps = %.2f A (DIM cap %.2f V, driver %.0f A)\n",
                    maxLoadAmps(), maxDimVolts(), kDriverRatedAmps);
    } else {
      const float amps = args.toFloat();
      if (!setMaxLoadAmps(amps)) {
        Serial.printf("ERR maxamps must be %.1f .. %.1f A\n", 0.5f, kDriverRatedAmps);
      } else {
        Serial.printf("OK maxLoadAmps -> %.2f A (DIM cap %.2f V, stored in NVS)\n",
                      maxLoadAmps(), maxDimVolts());
      }
    }
  } else if (cmd == "cal") {
    handleCal(args);
  } else {
    Serial.printf("Unknown command: '%s' (type 'help')\n", cmd.c_str());
  }
}

void SerialConsole::handleCal(const String &args) {
  int sp = args.indexOf(' ');
  String sub = (sp < 0) ? args : args.substring(0, sp);
  String rest = (sp < 0) ? "" : args.substring(sp + 1);
  sub.toLowerCase();
  rest.trim();

  if (sub == "hold") {
    dim_.setManualHold(true);
    Serial.println(F("CAL: manual hold ON (arbiter no longer drives PWM)"));
    Serial.println(F("     Put a voltmeter on the DIM line. Use 'cal duty <pct>' or 'cal step'."));
  } else if (sub == "release") {
    dim_.setManualHold(false);
    Serial.println(F("CAL: manual hold OFF (arbiter resumes control)"));
  } else if (sub == "duty") {
    if (!dim_.isManualHold()) dim_.setManualHold(true);
    float pct = rest.toFloat();
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    dim_.writeDutyFraction(pct / 100.0f);
    Serial.printf("CAL: duty = %.1f%% -> measure DIM volts now\n", pct);
  } else if (sub == "step") {
    if (!dim_.isManualHold()) dim_.setManualHold(true);
    const float pct = sweepIndex_ * 10.0f;
    dim_.writeDutyFraction(pct / 100.0f);
    Serial.printf("CAL step %d/10: duty = %.0f%%  -> record DIM volts (and clamp-meter amps if relay on)\n",
                  sweepIndex_, pct);
    if (sweepIndex_ < 10) {
      sweepIndex_++;
    } else {
      Serial.println(F("CAL: sweep complete. 'cal reset' to start over, 'cal release' when done."));
    }
  } else if (sub == "reset") {
    sweepIndex_ = 0;
    Serial.println(F("CAL: sweep index reset to 0"));
  } else {
    Serial.println(F("CAL usage: cal hold | release | duty <0-100> | step | reset"));
  }
}

void SerialConsole::printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  help                list commands"));
  Serial.println(F("  status              print controller state"));
  Serial.println(F("  level <0-100>       remote mode, set level %"));
  Serial.println(F("  on | off            relay output on/off"));
  Serial.println(F("  release             hand control back to the knob"));
  Serial.println(F("  maxamps [A]         show or set load cap (NVS, 0.5-22)"));
  Serial.println(F("  cal hold|release    suspend/resume arbiter PWM control"));
  Serial.println(F("  cal duty <0-100>    set raw PWM duty %"));
  Serial.println(F("  cal step            advance 0..100% sweep one notch"));
  Serial.println(F("  cal reset           reset sweep index"));
}

void SerialConsole::printStatus() {
  ControlArbiter::State st;
  arbiter_.snapshot(st);
  Serial.println(F("--- LumaCanopy status ---"));
  Serial.printf("  mode        : %s\n", ControlArbiter::modeName(st.mode));
  Serial.printf("  setpoint    : %.1f%% (user)\n", st.setpointPct);
  Serial.printf("  dim volts   : %.2f V (cap %.2f V, %.1f A)\n", dim_.dimVolts(),
                maxDimVolts(), maxLoadAmps());
  Serial.printf("  ramped      : %.1f%%\n", st.outputPercent);
  Serial.printf("  output on   : %s\n", st.outputOn ? "yes" : "no");
  Serial.printf("  relay       : %s\n", st.relayClosed ? "closed" : "open");
  Serial.printf("  knob pos    : %d\n",
                st.knobPosition >= 0 ? st.knobPosition + 1 : 0);
  Serial.printf("  lockout     : %s\n", st.lockout ? "YES" : "no");
  Serial.printf("  est. amps   : %.1f A (of %.1f A cap)\n", st.estimatedAmps,
                maxLoadAmps());
  Serial.printf("  manual hold : %s\n", dim_.isManualHold() ? "ON (cal)" : "off");
}

} // namespace luma
