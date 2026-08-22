// WiFiManager.cpp
// STATUS: 1 REAL

#include "WiFiManager.h"

bool WiFiManager::begin(const String& ssidPrefix, const String& password) {
  // Suffix the SSID with the last 3 bytes of the MAC so multiple pairs of
  // glasses on the workbench don't collide.
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  _ssid = ssidPrefix + "-" + String(suffix);

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(_ssid.c_str(), password.c_str());
  if (!ok) {
    Serial.println("[WiFiManager] softAP start failed");
    return false;
  }

  _controlServer.begin();
  _audioServer.begin();
  Serial.printf("[WiFiManager] AP '%s' up, IP %s\n", _ssid.c_str(), WiFi.softAPIP().toString().c_str());
  return true;
}

void WiFiManager::loop() {
  // Accept a control client if we don't have one.
  if (!_controlClient || !_controlClient.connected()) {
    WiFiClient incoming = _controlServer.available();
    if (incoming) {
      _controlClient = incoming;
      _lineBuffer = "";
      Serial.println("[WiFiManager] control client connected");
    }
  }
  // Accept an audio client if we don't have one.
  if (!_audioClient || !_audioClient.connected()) {
    WiFiClient incoming = _audioServer.available();
    if (incoming) {
      _audioClient = incoming;
      Serial.println("[WiFiManager] audio client connected");
    }
  }

  // Drain any available control-channel bytes into complete lines.
  while (_controlClient && _controlClient.connected() && _controlClient.available()) {
    char c = _controlClient.read();
    if (c == '\n') {
      _lineBuffer.trim();
      if (_lineBuffer.length() > 0 && _onLine) {
        _onLine(_lineBuffer);
      }
      _lineBuffer = "";
    } else if (c != '\r') {
      _lineBuffer += c;
      if (_lineBuffer.length() > 512) { // guard against a runaway line
        _lineBuffer = "";
      }
    }
  }
}

bool WiFiManager::sendControl(const String& line) {
  if (!_controlClient || !_controlClient.connected()) return false;
  _controlClient.print(line);
  _controlClient.print('\n');
  return true;
}

bool WiFiManager::sendAudioChunk(const uint8_t* data, size_t len) {
  if (!_audioClient || !_audioClient.connected()) return false;
  uint32_t lenLE = (uint32_t)len; // ESP32 is little-endian, matches protocol
  _audioClient.write(reinterpret_cast<const uint8_t*>(&lenLE), 4);
  _audioClient.write(data, len);
  return true;
}
