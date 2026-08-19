// ConfigManager.h
// STATUS: 1 REAL — needs the ArduinoJson library (see requirements.md).
//
// Reads/writes /OpenVisionEye/config/config.json on the SD card. No Wi-Fi
// internet credentials are ever stored here — the XIAO's own SoftAP doesn't
// need a password field for a client network, and this project has no
// internet mode in v1 (see docs/wifi_protocol.md and README "Security").

#pragma once
#include <Arduino.h>

struct OpenVisionEyeConfig {
  String language = "en";              // "en" (recommended for v1), "ca", "es"
  int volume = 70;                     // 0-100
  bool wakeWordEnabled = true;         // manual-trigger stub, see WakeWordEngine.h
  String audioMode = "wifi_to_bt";     // only mode implemented in v1
  bool batteryAdcDividerEnabled = false; // see BatteryManager.h — off unless you wired it
  int batteryAdcPin = -1;              // -1 = use the board's own A0 macro (recommended).
                                        // Only override with a raw GPIO number if you've
                                        // confirmed it on your specific board revision —
                                        // community sources disagree on the exact GPIO
                                        // number behind "A0" across XIAO ESP32-S3 batches.
  String apSsidPrefix = "OpenVisionEye";
  // Local Wi-Fi AP password only (device-to-device, never leaves the two
  // boards). Not an "internet credential" — see README.md "Security".
  // Change this in config.json before relying on it for anything sensitive.
  String apPassword = "glasses1234";
};

class ConfigManager {
public:
  bool begin();  // loads from SD, or writes+loads defaults if missing
  const OpenVisionEyeConfig& get() const { return _config; }
  bool save();

  // Mutators used by CommandManager when a setting changes at runtime
  // (e.g. VOLUME:<n> from the Wi-Fi protocol).
  void setVolume(int v);

private:
  OpenVisionEyeConfig _config;
  static constexpr const char* kPath = "/OpenVisionEye/config/config.json";
  bool loadFromDisk();
  bool writeDefaults();
};
