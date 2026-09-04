#pragma once

#include <Arduino.h>

#include "ControlArbiter.h"
#include "DimOutput.h"

namespace luma {

// USB-serial command shell for phase-1 bring-up and calibration.
//
// Commands:
//   help                 - list commands
//   status               - print full controller state
//   level <0-100>        - remote mode, set level %
//   on | off             - relay on/off
//   release              - hand control back to the knob
//   cal hold | release   - suspend/resume arbiter control of the PWM
//   cal duty <0-100>     - set raw PWM duty % (enables hold)
//   cal step             - advance the 0..100% sweep one notch and print target
//   cal reset            - reset the sweep index
//   maxamps [A]          - show or set the load cap (persisted in NVS)
class SerialConsole {
public:
  SerialConsole(ControlArbiter &arbiter, DimOutput &dim);
  void begin();
  void update();

private:
  void handleLine(const String &line);
  void handleCal(const String &args);
  void printHelp();
  void printStatus();

  ControlArbiter &arbiter_;
  DimOutput &dim_;
  String buf_;
  int sweepIndex_ = 0;
};

} // namespace luma
