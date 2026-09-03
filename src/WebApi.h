#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "ControlArbiter.h"
#include "WifiControl.h"

namespace luma {

// Locally-served REST API and the embedded single-page web app.
// Uses the ESP32 core WebServer (no ESPAsyncWebServer). The UI polls
// GET /api/state. All writes require the NVS-stored PIN (header X-Pin or a
// "pin" form field).
class WebApi {
public:
  WebApi(ControlArbiter &arbiter, WifiControl &wifi);
  void begin();
  void loop();

private:
  bool authorized() const;
  String currentStateJson();
  void registerRoutes();

  void handleIndex();
  void handleManifest();
  void handleState();
  void handleLevel();
  void handleOutput();
  void handleRelease();
  void handleWifi();
  void handleNotFound();

  ControlArbiter &arbiter_;
  WifiControl &wifi_;
  WebServer server_;
};

} // namespace luma
