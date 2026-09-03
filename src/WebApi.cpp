#include "WebApi.h"

#include <ArduinoJson.h>

#include "Config.h"
#include "web_index.h"

namespace luma {

WebApi::WebApi(ControlArbiter &arbiter, WifiControl &wifi)
    : arbiter_(arbiter), wifi_(wifi), server_(80), ws_("/ws") {}

bool WebApi::authorized(AsyncWebServerRequest *req) const {
  String provided;
  if (req->hasHeader("X-Pin")) {
    provided = req->header("X-Pin");
  } else if (req->hasParam("pin", true)) {
    provided = req->getParam("pin", true)->value();
  }
  return provided.length() > 0 && provided == wifi_.pin();
}

String WebApi::serializeState(const ControlArbiter::State &st) {
  JsonDocument doc;
  doc["mode"] = ControlArbiter::modeName(st.mode);
  doc["setpointPct"] = st.setpointPct;
  doc["outputPercent"] = st.outputPercent;
  doc["outputOn"] = st.outputOn;
  doc["relayClosed"] = st.relayClosed;
  doc["knobPosition"] = st.knobPosition;
  doc["lockout"] = st.lockout;
  doc["maxLevelPct"] = st.maxLevelPct;
  doc["estimatedAmps"] = st.estimatedAmps;
  doc["wifiConnected"] = st.wifiConnected;
  doc["wifiMode"] = st.wifiMode;
  doc["ip"] = st.ip;
  String out;
  serializeJson(doc, out);
  return out;
}

void WebApi::registerRoutes() {
  server_.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    AsyncWebServerResponse *res =
        req->beginResponse(200, "text/html", (const uint8_t *)kIndexHtml,
                           strlen_P(kIndexHtml));
    req->send(res);
  });

  server_.on("/manifest.webmanifest", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/manifest+json", kManifest);
  });

  server_.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest *req) {
    ControlArbiter::State st;
    arbiter_.snapshot(st);
    st.wifiConnected = wifi_.isConnected();
    st.wifiMode = wifi_.modeString();
    st.ip = wifi_.ipString();
    req->send(200, "application/json", serializeState(st));
  });

  server_.on("/api/level", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!authorized(req)) { req->send(401, "text/plain", "unauthorized"); return; }
    if (!req->hasParam("pct", true)) { req->send(400, "text/plain", "missing pct"); return; }
    const float pct = req->getParam("pct", true)->value().toFloat();
    arbiter_.requestRemoteLevel(pct);
    req->send(200, "text/plain", "ok");
  });

  server_.on("/api/output", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!authorized(req)) { req->send(401, "text/plain", "unauthorized"); return; }
    if (!req->hasParam("on", true)) { req->send(400, "text/plain", "missing on"); return; }
    const bool on = req->getParam("on", true)->value().toInt() != 0;
    arbiter_.requestOutput(on);
    req->send(200, "text/plain", "ok");
  });

  server_.on("/api/release", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!authorized(req)) { req->send(401, "text/plain", "unauthorized"); return; }
    arbiter_.requestRelease();
    req->send(200, "text/plain", "ok");
  });

  server_.on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *req) {
    if (!authorized(req)) { req->send(401, "text/plain", "unauthorized"); return; }
    if (!req->hasParam("ssid", true)) { req->send(400, "text/plain", "missing ssid"); return; }
    const String ssid = req->getParam("ssid", true)->value();
    const String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";
    wifi_.saveCredentials(ssid, pass);
    req->send(200, "text/plain", "ok");
    // Reboot shortly after responding so the new credentials take effect.
    req->onDisconnect([]() { delay(200); ESP.restart(); });
  });

  server_.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "not found");
  });

  ws_.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                 AwsEventType type, void *arg, uint8_t *data, size_t len) {
    (void)server; (void)client; (void)arg; (void)data; (void)len; (void)type;
  });
  server_.addHandler(&ws_);
}

void WebApi::begin() {
  registerRoutes();
  server_.begin();
  Serial.println(F("WebApi: server started on :80"));
}

void WebApi::loop() {
  const uint32_t now = millis();
  if (now - lastCleanupMs_ >= 1000) {
    lastCleanupMs_ = now;
    ws_.cleanupClients();
  }
}

void WebApi::broadcastState(const ControlArbiter::State &st) {
  if (ws_.count() == 0) return;
  ws_.textAll(serializeState(st));
}

} // namespace luma
