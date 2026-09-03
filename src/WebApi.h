#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "ControlArbiter.h"
#include "WifiControl.h"

namespace luma {

// Locally-served REST + WebSocket API and the embedded single-page web app.
//
// Serves the UI at "/", exposes the control endpoints under "/api", and pushes
// live state over "/ws". All writes require the NVS-stored PIN (header X-Pin or
// a "pin" form field).
class WebApi {
public:
  WebApi(ControlArbiter &arbiter, WifiControl &wifi);
  void begin();
  void loop();

  // Push a state snapshot to all connected WebSocket clients.
  void broadcastState(const ControlArbiter::State &st);

private:
  bool authorized(AsyncWebServerRequest *req) const;
  static String serializeState(const ControlArbiter::State &st);
  void registerRoutes();

  ControlArbiter &arbiter_;
  WifiControl &wifi_;
  AsyncWebServer server_;
  AsyncWebSocket ws_;
  uint32_t lastCleanupMs_ = 0;
};

} // namespace luma
