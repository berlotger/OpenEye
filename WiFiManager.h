// WiFiManager.h
// STATUS: 1 REAL — standard Arduino WiFi.h SoftAP + WiFiServer, per the
// offline requirement (no router/internet, XIAO hosts its own network).
// See docs/wifi_protocol.md for the two-port protocol this implements.

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include "ProtocolDefs.h"

using ControlLineHandler = std::function<void(const String& line)>;

class WiFiManager {
public:
  bool begin(const String& ssidPrefix, const String& password);

  // Call every loop() iteration — accepts/holds the single expected
  // ESP32 Audio client on both ports, and dispatches any full control lines
  // received to `onControlLine`.
  void loop();

  void setControlLineHandler(ControlLineHandler handler) { _onLine = handler; }

  // Sends one control-channel line (docs/wifi_protocol.md). Adds the
  // trailing '\n' for you.
  bool sendControl(const String& line);

  // Sends one length-prefixed PCM chunk on the audio channel.
  bool sendAudioChunk(const uint8_t* data, size_t len);

  bool audioClientConnected() const { return _audioClient && _audioClient.connected(); }
  bool controlClientConnected() const { return _controlClient && _controlClient.connected(); }

  String apIpString() const { return WiFi.softAPIP().toString(); }
  // Real check, not a hardcoded "OK" — used by runDiagnostics() so the
  // Wi-Fi AP line reflects actual state (spec §33: no false positives).
  bool isApActive() const { return WiFi.getMode() & WIFI_AP; }
  String ssid() const { return _ssid; }

private:
  WiFiServer _controlServer{Proto::CONTROL_PORT};
  WiFiServer _audioServer{Proto::AUDIO_PORT};
  WiFiClient _controlClient;
  WiFiClient _audioClient;
  String _ssid;
  String _lineBuffer;
  ControlLineHandler _onLine;
};
