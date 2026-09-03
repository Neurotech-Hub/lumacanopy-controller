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
// Libraries: Hublink-Node-Raven, ArduinoJson, and (for Wi-Fi) the maintained
// ESP32Async fork -- "ESP Async WebServer" + "Async TCP".

#include <Arduino.h>
#include <HublinkNodeRaven.h>

#include "src/Config.h"
#include "src/DimOutput.h"
#include "src/RelayControl.h"
#include "src/RotarySwitch.h"
#include "src/KillSwitch.h"
#include "src/Indicators.h"
#include "src/ControlArbiter.h"
#include "src/SerialConsole.h"
#include "src/WifiControl.h"
#include "src/WebApi.h"

raven::HublinkNode node;

luma::DimOutput dimOutput;
luma::RelayControl relay;
luma::RotarySwitch rotary;
luma::KillSwitch killSwitch;
luma::Indicators indicators;

luma::ControlArbiter arbiter(dimOutput, relay, rotary, killSwitch, indicators);
luma::SerialConsole console(arbiter, dimOutput);
luma::WifiControl wifi;
luma::WebApi webApi(arbiter, wifi);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("LumaCanopy Controller booting..."));

  // beginHardware() resets our GPIOs (A5/A4 etc.) to INPUT, so it MUST run
  // before we configure the dimming/relay pins.
  node.beginHardware();

  dimOutput.begin();
  relay.begin();
  rotary.begin();
  killSwitch.begin();
  indicators.begin();
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

  // Push live state to any connected browsers roughly twice a second, plus on
  // every state change (handled inside the arbiter/webApi).
  static uint32_t lastPush = 0;
  const uint32_t now = millis();
  if (now - lastPush >= 500) {
    lastPush = now;
    luma::ControlArbiter::State st;
    arbiter.snapshot(st);
    st.wifiConnected = wifi.isConnected();
    st.ip = wifi.ipString();
    st.wifiMode = wifi.modeString();
    webApi.broadcastState(st);
  }
}
