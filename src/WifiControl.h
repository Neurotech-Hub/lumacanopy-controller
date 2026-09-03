#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace luma {

// Wi-Fi bring-up and credential/PIN storage (NVS).
//
// Tries stored STA credentials; on failure (or if none stored) falls back to a
// WPA2-protected SoftAP that serves the UI and a Wi-Fi setup page. Publishes
// mDNS as lumacanopy.local in both modes.
class WifiControl {
public:
  void begin();
  void update();

  bool isConnected() const;          // STA associated
  bool isAccessPoint() const { return apActive_; }
  String ipString() const;
  String modeString() const;         // "sta" | "ap"
  String ssid() const { return ssid_; }

  // Persisted settings.
  void saveCredentials(const String &ssid, const String &pass); // caller reboots
  String pin();
  void setPin(const String &pin);

private:
  bool startStation();
  void startAccessPoint();
  void startMdns();

  Preferences prefs_;
  String ssid_;
  bool apActive_ = false;
  bool mdnsUp_ = false;
  uint32_t lastRetryMs_ = 0;
};

} // namespace luma
