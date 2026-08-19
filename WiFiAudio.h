// WiFiAudio.h
// STATUS: 1 REAL — WiFi station mode, connects to the XIAO's SoftAP and
// then to its two TCP servers (docs/wifi_protocol.md). Scans for the SSID
// by prefix since the XIAO appends a MAC-derived suffix (see
// XIAO_OpenVisionEye/WiFiManager.cpp).

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include "ProtocolDefs.h"

using ControlLineHandler = std::function<void(const String& line)>;
using PcmChunkHandler = std::function<void(const uint8_t* data, size_t len)>;

class WiFiAudio {
public:
  // Connects to Wi-Fi and both TCP ports. Blocks (with a bounded retry loop
  // and Serial progress prints) until the AP is found, since there is
  // nothing useful to do before that succeeds.
  bool begin(const char* ssidPrefix, const char* password, const char* apIp,
             uint32_t connectTimeoutMs = 20000);

  // Call every loop() iteration. Reconnects the TCP sockets if they drop
  // (Wi-Fi association itself is handled by the IDF's own auto-reconnect).
  void loop();

  void setControlLineHandler(ControlLineHandler h) { _onLine = h; }
  void setPcmChunkHandler(PcmChunkHandler h) { _onPcm = h; }

  bool sendControl(const String& line);
  bool isControlConnected() const { return _controlClient && _controlClient.connected(); }
  bool isAudioConnected() const { return _audioClient && _audioClient.connected(); }

private:
  String _apIp;
  uint16_t _controlPort = Proto::CONTROL_PORT;
  uint16_t _audioPort = Proto::AUDIO_PORT;

  WiFiClient _controlClient;
  WiFiClient _audioClient;
  String _lineBuffer;
  ControlLineHandler _onLine;
  PcmChunkHandler _onPcm;

  // Audio-channel length-prefix parsing state machine (see
  // docs/wifi_protocol.md — [uint32 len][payload], repeated).
  enum class AudioParseState { WAIT_LENGTH, WAIT_PAYLOAD };
  AudioParseState _audioState = AudioParseState::WAIT_LENGTH;
  uint8_t _lenBuf[4];
  size_t _lenBufFill = 0;
  uint32_t _expectedPayloadLen = 0;
  static constexpr size_t MAX_CHUNK_BYTES = 8192; // sanity bound, see loop()

  void pumpControl();
  void pumpAudio();
  bool reconnectSockets();
};
