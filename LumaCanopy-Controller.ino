// LumaCanopy Controller
//
// Firmware for a Hublink-Node-Raven (ESP32-S3) that dims two Mean Well
// HLG-320H-12B drivers through a 3.3V-PWM -> 0-10V converter board, with a hard
// relay cutoff, an 8-position knob, a maintained kill switch, two case
// indicator LEDs, and a locally-served web app for remote control.
//
// Board setup (Arduino IDE):
//   - Board: "ESP32S3 Dev Module"
//   - Tools -> USB CDC On Boot: Enabled
//   - Enter boot mode before flashing (hold Boot, tap Reset, release Boot).
//
// Libraries: ArduinoJson. The web UI uses the ESP32 core WebServer.

#include <Arduino.h>

#include "src/Config.h"
#include "src/DimOutput.h"
#include "src/RelayControl.h"
#include "src/RotarySwitch.h"
#include "src/KillSwitch.h"
#include "src/Indicators.h"
#include "src/SlotStore.h"
#include "src/ControlArbiter.h"
#include "src/SerialConsole.h"
#include "src/WifiControl.h"
#include "src/WebApi.h"

luma::DimOutput dimOutput;
luma::RelayControl relay;
luma::RotarySwitch rotary;
luma::KillSwitch killSwitch;
luma::Indicators indicators;
luma::SlotStore slots;

luma::ControlArbiter arbiter(dimOutput, relay, rotary, killSwitch, indicators,
                             slots);
luma::SerialConsole console(arbiter, dimOutput);
luma::WifiControl wifi;
luma::WebApi webApi(arbiter, wifi, slots);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("LumaCanopy Controller booting..."));

  luma::beginBoard();

  dimOutput.begin();
  relay.begin();
  rotary.begin();
  killSwitch.begin();
  indicators.begin();
  slots.begin(); // must precede arbiter.begin(); it reads the boot slot
  arbiter.begin();
  console.begin();

  wifi.begin();
  webApi.begin();

  Serial.println(F("Ready. Type 'help' for serial commands."));
}

void loop() {
  arbiter.update();
  console.update();
  wifi.update();
  webApi.loop();
}
