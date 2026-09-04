#include "WebApi.h"

#include <ArduinoJson.h>

#include "Config.h"
#include "LoadCap.h"
#include "web_index.h"

namespace luma {

WebApi::WebApi(ControlArbiter &arbiter, WifiControl &wifi, SlotStore &slots)
    : arbiter_(arbiter), wifi_(wifi), slots_(slots), server_(80) {}

bool WebApi::authorized() const {
  String provided = server_.header("X-Pin");
  if (!provided.length() && server_.hasArg("pin")) {
    provided = server_.arg("pin");
  }
  return provided.length() > 0 && provided == wifi_.pin();
}

String WebApi::currentStateJson() {
  ControlArbiter::State st;
  arbiter_.snapshot(st);
  st.wifiConnected = wifi_.isConnected();
  st.wifiMode = wifi_.modeString();
  st.ip = wifi_.ipString();

  JsonDocument doc;
  doc["mode"] = ControlArbiter::modeName(st.mode);
  doc["setpointPct"] = st.setpointPct;
  doc["instantPct"] = st.instantPct;
  doc["outputPercent"] = st.outputPercent;
  doc["outputOn"] = st.outputOn;
  doc["relayClosed"] = st.relayClosed;
  doc["knobPosition"] = st.knobPosition;
  doc["activeSlot"] = st.activeSlot;
  doc["effect"] = effectName(st.effect);
  doc["maxBlinkHz"] = kMaxBlinkHz;
  doc["lockout"] = st.lockout;
  doc["maxLevelPct"] = st.maxLevelPct;
  doc["maxLoadAmps"] = maxLoadAmps();
  doc["maxDimVolts"] = maxDimVolts();
  doc["estimatedAmps"] = st.estimatedAmps;
  doc["wifiConnected"] = st.wifiConnected;
  doc["wifiMode"] = st.wifiMode;
  doc["ip"] = st.ip;
  String out;
  serializeJson(doc, out);
  return out;
}

void WebApi::handleIndex() {
  server_.send_P(200, "text/html", kIndexHtml);
}

void WebApi::handleManifest() {
  server_.send_P(200, "application/manifest+json", kManifest);
}

void WebApi::handleState() {
  server_.send(200, "application/json", currentStateJson());
}

void WebApi::handleLevel() {
  if (!authorized()) {
    server_.send(401, "text/plain", "unauthorized");
    return;
  }
  if (!server_.hasArg("pct")) {
    server_.send(400, "text/plain", "missing pct");
    return;
  }
  arbiter_.requestRemoteLevel(server_.arg("pct").toFloat());
  server_.send(200, "text/plain", "ok");
}

void WebApi::handleOutput() {
  if (!authorized()) {
    server_.send(401, "text/plain", "unauthorized");
    return;
  }
  if (!server_.hasArg("on")) {
    server_.send(400, "text/plain", "missing on");
    return;
  }
  arbiter_.requestOutput(server_.arg("on").toInt() != 0);
  server_.send(200, "text/plain", "ok");
}

void WebApi::handleRelease() {
  if (!authorized()) {
    server_.send(401, "text/plain", "unauthorized");
    return;
  }
  arbiter_.requestRelease();
  server_.send(200, "text/plain", "ok");
}

void WebApi::handleWifi() {
  if (!authorized()) {
    server_.send(401, "text/plain", "unauthorized");
    return;
  }
  if (!server_.hasArg("ssid")) {
    server_.send(400, "text/plain", "missing ssid");
    return;
  }
  const String ssid = server_.arg("ssid");
  const String pass = server_.hasArg("pass") ? server_.arg("pass") : "";
  wifi_.saveCredentials(ssid, pass);
  server_.send(200, "text/plain", "ok");
  delay(250);
  ESP.restart();
}

void WebApi::handleSlotsGet() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < SlotStore::kCount; ++i) {
    const SlotConfig c = slots_.get(i);
    JsonObject o = arr.add<JsonObject>();
    o["i"] = i;
    o["kind"] = effectName(c.kind);
    o["highPct"] = c.highPct;
    o["lowPct"] = c.lowPct;
    o["onMs"] = c.onMs;
    o["offMs"] = c.offMs;
  }
  String out;
  serializeJson(doc, out);
  server_.send(200, "application/json", out);
}

void WebApi::handleSlotsPost() {
  if (!authorized()) {
    server_.send(401, "text/plain", "unauthorized");
    return;
  }
  if (!server_.hasArg("i")) {
    server_.send(400, "text/plain", "missing i");
    return;
  }
  const int index = server_.arg("i").toInt();
  if (index < 0 || index >= SlotStore::kCount) {
    server_.send(400, "text/plain", "bad slot index");
    return;
  }

  // Start from the stored program so a partial write only changes what it names.
  SlotConfig c = slots_.get(index);
  if (server_.hasArg("kind")) {
    EffectKind k;
    if (!effectFromName(server_.arg("kind"), k)) {
      server_.send(400, "text/plain", "bad kind");
      return;
    }
    c.kind = k;
  }
  if (server_.hasArg("high")) c.highPct = server_.arg("high").toFloat();
  if (server_.hasArg("low")) c.lowPct = server_.arg("low").toFloat();
  if (server_.hasArg("on")) c.onMs = (uint16_t)server_.arg("on").toInt();
  if (server_.hasArg("off")) c.offMs = (uint16_t)server_.arg("off").toInt();

  // set() clamps to the level and rate caps, so the reply reports what was
  // actually stored rather than what was asked for.
  slots_.set(index, c);
  const SlotConfig stored = slots_.get(index);

  JsonDocument doc;
  doc["i"] = index;
  doc["kind"] = effectName(stored.kind);
  doc["highPct"] = stored.highPct;
  doc["lowPct"] = stored.lowPct;
  doc["onMs"] = stored.onMs;
  doc["offMs"] = stored.offMs;
  String out;
  serializeJson(doc, out);
  server_.send(200, "application/json", out);
}

void WebApi::handleSlotSelect() {
  if (!authorized()) {
    server_.send(401, "text/plain", "unauthorized");
    return;
  }
  if (!server_.hasArg("i")) {
    server_.send(400, "text/plain", "missing i");
    return;
  }
  const int index = server_.arg("i").toInt();
  if (index < 0 || index >= SlotStore::kCount) {
    server_.send(400, "text/plain", "bad slot index");
    return;
  }
  arbiter_.requestSlot(index);
  server_.send(200, "text/plain", "ok");
}

void WebApi::handleSlotsReset() {
  if (!authorized()) {
    server_.send(401, "text/plain", "unauthorized");
    return;
  }
  slots_.resetDefaults();
  server_.send(200, "text/plain", "ok");
}

void WebApi::handleNotFound() {
  server_.send(404, "text/plain", "not found");
}

void WebApi::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleIndex(); });
  server_.on("/manifest.webmanifest", HTTP_GET, [this]() { handleManifest(); });
  server_.on("/api/state", HTTP_GET, [this]() { handleState(); });
  server_.on("/api/level", HTTP_POST, [this]() { handleLevel(); });
  server_.on("/api/output", HTTP_POST, [this]() { handleOutput(); });
  server_.on("/api/release", HTTP_POST, [this]() { handleRelease(); });
  server_.on("/api/slots", HTTP_GET, [this]() { handleSlotsGet(); });
  server_.on("/api/slots", HTTP_POST, [this]() { handleSlotsPost(); });
  server_.on("/api/slots/reset", HTTP_POST, [this]() { handleSlotsReset(); });
  server_.on("/api/slot", HTTP_POST, [this]() { handleSlotSelect(); });
  server_.on("/api/wifi", HTTP_POST, [this]() { handleWifi(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

void WebApi::begin() {
  const char *headerKeys[] = {"X-Pin"};
  server_.collectHeaders(headerKeys, 1);
  registerRoutes();
  server_.begin();
  Serial.println(F("WebApi: server started on :80"));
}

void WebApi::loop() { server_.handleClient(); }

} // namespace luma
