#include "WifiControl.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "Config.h"

namespace luma {

namespace {
constexpr char kPrefsNamespace[] = "luma";
constexpr uint32_t kStaRetryIntervalMs = 30000;
} // namespace

void WifiControl::begin() {
  prefs_.begin(kPrefsNamespace, false);
  // Seed the default write-auth PIN on first boot.
  if (!prefs_.isKey("pin")) {
    prefs_.putString("pin", luma::kDefaultPin);
  }
  ssid_ = prefs_.getString("ssid", "");
  const String pass = prefs_.getString("pass", "");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  bool connected = false;
  if (ssid_.length() > 0) {
    connected = startStation();
  }
  if (!connected) {
    startAccessPoint();
  }
  startMdns();
}

bool WifiControl::startStation() {
  const String pass = prefs_.getString("pass", "");
  Serial.printf("WiFi: connecting to '%s'...\n", ssid_.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_.c_str(), pass.c_str());

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < luma::kStaConnectTimeoutMs) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    apActive_ = false;
    Serial.printf("WiFi: connected, IP %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println(F("WiFi: STA connect timed out"));
  return false;
}

void WifiControl::startAccessPoint() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ap[32];
  snprintf(ap, sizeof(ap), "%s-%02X%02X", luma::kApPrefix, mac[4], mac[5]);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap, luma::kApPassword);
  apActive_ = true;
  ssid_ = ap;
  Serial.printf("WiFi: SoftAP '%s', IP %s\n", ap,
                WiFi.softAPIP().toString().c_str());
}

void WifiControl::startMdns() {
  if (mdnsUp_) return;
  if (MDNS.begin(luma::kMdnsHost)) {
    MDNS.addService("http", "tcp", 80);
    mdnsUp_ = true;
    Serial.printf("mDNS: http://%s.local\n", luma::kMdnsHost);
  } else {
    Serial.println(F("mDNS: start failed"));
  }
}

void WifiControl::update() {
  // In STA mode, periodically retry if the link drops. Hold the last commanded
  // output regardless (the arbiter does not depend on Wi-Fi).
  if (!apActive_ && WiFi.status() != WL_CONNECTED) {
    const uint32_t now = millis();
    if (now - lastRetryMs_ >= kStaRetryIntervalMs) {
      lastRetryMs_ = now;
      Serial.println(F("WiFi: link down, retrying..."));
      WiFi.reconnect();
    }
  }
}

bool WifiControl::isConnected() const {
  return !apActive_ && WiFi.status() == WL_CONNECTED;
}

String WifiControl::ipString() const {
  if (apActive_) return WiFi.softAPIP().toString();
  return WiFi.localIP().toString();
}

String WifiControl::modeString() const { return apActive_ ? "ap" : "sta"; }

void WifiControl::saveCredentials(const String &ssid, const String &pass) {
  prefs_.putString("ssid", ssid);
  prefs_.putString("pass", pass);
  Serial.printf("WiFi: saved credentials for '%s' (reboot to apply)\n",
                ssid.c_str());
}

String WifiControl::pin() { return prefs_.getString("pin", luma::kDefaultPin); }

void WifiControl::setPin(const String &pin) { prefs_.putString("pin", pin); }

} // namespace luma
