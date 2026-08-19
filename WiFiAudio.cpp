// WiFiAudio.cpp
// STATUS: 1 REAL

#include "WiFiAudio.h"
#include <string.h>

bool WiFiAudio::begin(const char* ssidPrefix, const char* password, const char* apIp,
                       uint32_t connectTimeoutMs) {
  _apIp = apIp;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("[WiFiAudio] scanning for SoftAP starting with \"" + String(ssidPrefix) + "\"...");
  unsigned long start = millis();
  String foundSsid;

  while (millis() - start < connectTimeoutMs) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.startsWith(ssidPrefix)) {
        foundSsid = ssid;
        break;
      }
    }
    WiFi.scanDelete();
    if (foundSsid.length()) break;
    Serial.println("[WiFiAudio] not found yet, retrying...");
    delay(1000);
  }

  if (foundSsid.length() == 0) {
    Serial.println("[WiFiAudio] no matching SoftAP found within timeout");
    return false;
  }

  Serial.println("[WiFiAudio] connecting to " + foundSsid);
  WiFi.begin(foundSsid.c_str(), password);

  while (WiFi.status() != WL_CONNECTED && millis() - start < connectTimeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFiAudio] Wi-Fi association failed");
    return false;
  }

  Serial.println("[WiFiAudio] connected, IP " + WiFi.localIP().toString());
  return reconnectSockets();
}

bool WiFiAudio::reconnectSockets() {
  bool ok = true;
  if (!_controlClient.connected()) {
    if (_controlClient.connect(_apIp.c_str(), _controlPort)) {
      Serial.println("[WiFiAudio] control channel connected");
    } else {
      Serial.println("[WiFiAudio] control channel connect failed");
      ok = false;
    }
  }
  if (!_audioClient.connected()) {
    if (_audioClient.connect(_apIp.c_str(), _audioPort)) {
      Serial.println("[WiFiAudio] audio channel connected");
      _audioState = AudioParseState::WAIT_LENGTH;
      _lenBufFill = 0;
    } else {
      Serial.println("[WiFiAudio] audio channel connect failed");
      ok = false;
    }
  }
  return ok;
}

void WiFiAudio::loop() {
  if (WiFi.status() != WL_CONNECTED) return; // IDF handles STA reconnection itself

  static unsigned long lastRetry = 0;
  if ((!_controlClient.connected() || !_audioClient.connected()) && millis() - lastRetry > 2000) {
    lastRetry = millis();
    reconnectSockets();
  }

  pumpControl();
  pumpAudio();
}

void WiFiAudio::pumpControl() {
  while (_controlClient && _controlClient.connected() && _controlClient.available()) {
    char c = _controlClient.read();
    if (c == '\n') {
      _lineBuffer.trim();
      if (_lineBuffer.length() > 0 && _onLine) _onLine(_lineBuffer);
      _lineBuffer = "";
    } else if (c != '\r') {
      _lineBuffer += c;
      if (_lineBuffer.length() > 512) _lineBuffer = "";
    }
  }
}

void WiFiAudio::pumpAudio() {
  if (!_audioClient || !_audioClient.connected()) return;

  // Bounded amount of work per loop() call so we never starve pumpControl()
  // or the Arduino core's own Wi-Fi housekeeping.
  int budget = 4096;
  while (_audioClient.available() && budget > 0) {
    if (_audioState == AudioParseState::WAIT_LENGTH) {
      int b = _audioClient.read();
      if (b < 0) break;
      _lenBuf[_lenBufFill++] = (uint8_t)b;
      budget--;
      if (_lenBufFill == 4) {
        memcpy(&_expectedPayloadLen, _lenBuf, 4);
        _lenBufFill = 0;
        if (_expectedPayloadLen == 0 || _expectedPayloadLen > MAX_CHUNK_BYTES) {
          Serial.printf("[WiFiAudio] rejecting implausible chunk length %u\n", (unsigned)_expectedPayloadLen);
          // Resync by dropping the connection; a garbled length means the
          // stream framing is lost and can't be safely recovered in place.
          _audioClient.stop();
          break;
        }
        _audioState = AudioParseState::WAIT_PAYLOAD;
      }
    } else {
      uint8_t buf[512];
      int want = min((uint32_t)sizeof(buf), _expectedPayloadLen);
      int got = _audioClient.read(buf, want);
      if (got <= 0) break;
      if (_onPcm) _onPcm(buf, got);
      _expectedPayloadLen -= got;
      budget -= got;
      if (_expectedPayloadLen == 0) {
        _audioState = AudioParseState::WAIT_LENGTH;
      }
    }
  }
}

bool WiFiAudio::sendControl(const String& line) {
  if (!_controlClient || !_controlClient.connected()) return false;
  _controlClient.print(line);
  _controlClient.print('\n');
  return true;
}
