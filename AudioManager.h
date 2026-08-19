// AudioManager.h
// STATUS: mixed —
//   mic capture:            1 REAL (ESP_I2S.h, built into the Arduino ESP32 core)
//   SD file streaming:      1 REAL (plain file I/O)
//   actual sound out:       lives on the ESP32 Audio board — see
//                            docs/architecture.md "Audio: how music/TTS
//                            actually gets to your ears" for why the XIAO
//                            has no local speaker/DAC path in this BOM.
//
// Mic pins verified: PDM CLK=GPIO42, PDM DATA=GPIO41 (docs/hardware.md §1.3).

#pragma once
#include <Arduino.h>
#include <functional>
#include <ESP_I2S.h>
#include "SDManager.h"

// Per your requirement: mic source is not hardcoded.
enum class AudioInputSource {
  MIC_XIAO,      // 1 REAL — onboard PDM mic
  MIC_HEADSET,   // NOT IMPLEMENTED — see docs/hardware.md §2.4 (A2DP has no return audio path)
  MIC_EXTERNAL,  // NOT IMPLEMENTED — placeholder for a future wired mic
};

// Callback the app wires up to actually transmit PCM bytes (normally
// WiFiManager::sendAudioChunk). Decoupled so AudioManager doesn't need to
// know about Wi-Fi/sockets.
using PcmSinkFn = std::function<void(const uint8_t* data, size_t len)>;

class AudioManager {
public:
  static constexpr int PDM_CLK_PIN = 42;
  static constexpr int PDM_DATA_PIN = 41;
  static constexpr uint32_t SAMPLE_RATE = 16000;

  bool begin(SDManager* sd);

  void setInputSource(AudioInputSource src) { _source = src; } // only MIC_XIAO works in v1
  AudioInputSource inputSource() const { return _source; }

  // Streams SAMPLE_RATE mono 16-bit PCM from the onboard mic to `sink` for
  // `durationMs` milliseconds. Used by "start recording" / mic-to-BT.
  // Returns false immediately if inputSource() isn't MIC_XIAO.
  bool streamMicTo(PcmSinkFn sink, uint32_t durationMs);

  // Records the mic to a WAV file on SD (self-contained, no network needed).
  // Used by "record audio" -> /OpenVisionEye/audio/.
  bool recordToFile(const String& path, uint32_t durationMs);

  // Reads a mono 16-bit 16kHz WAV file from SD and streams its PCM payload
  // to `sink` in chunks. Used for music playback and canned TTS prompts.
  // Returns false if the file is missing or not a WAV in the expected format
  // (logs why — does not attempt to transcode).
  bool streamFileTo(const String& path, PcmSinkFn sink);

private:
  SDManager* _sd = nullptr;
  I2SClass _i2s;
  AudioInputSource _source = AudioInputSource::MIC_XIAO;
  bool _i2sReady = false;

  bool writeWavHeader(File& f, uint32_t dataBytes);
};
