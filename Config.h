// Config.h
// STATUS: 1 REAL
//
// Board: any CLASSIC ESP32 (Xtensa, dual-core) — NOT S2/S3/C3/C6.
// See docs/hardware.md §2.1 for why this is a hard requirement, verified
// against the ESP32-A2DP library's own compile-time check.
//
// Arduino IDE board setting: "ESP32 Dev Module" (or your specific classic
// ESP32 board entry, e.g. a classic XIAO ESP32 if that's what you use).

#pragma once

namespace AudioHubConfig {
  // Must match the XIAO's ConfigManager::apSsidPrefix (default
  // "OpenVisionEye" — the XIAO appends a MAC-derived suffix, so we match by
  // prefix, not exact SSID).
  constexpr const char* AP_SSID_PREFIX = "OpenVisionEye";

  // Must match the XIAO's ConfigManager::apPassword. Change both together.
  constexpr const char* AP_PASSWORD = "glasses1234";

  // Default IP the Arduino/ESP-IDF SoftAP stack assigns itself unless
  // reconfigured — the XIAO's WiFiManager does not change it, so this is
  // safe to hardcode rather than discover at runtime.
  constexpr const char* XIAO_AP_IP = "192.168.4.1";

  // Bluetooth name this board advertises/searches for when connecting to
  // your headphones. BluetoothA2DPSource::start(name) does a name-based
  // connect — set this to your headphones' Bluetooth name, or leave blank
  // and use the discovery-callback approach described in
  // docs/installation.md if you'd rather pick from a scan.
  constexpr const char* HEADSET_BT_NAME = "";
}
