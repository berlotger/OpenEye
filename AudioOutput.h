// AudioOutput.h
// STATUS: 1 REAL — needs the ESP32-A2DP library (see requirements.md).
//
// Bridges the mono 16-bit 16kHz PCM arriving over Wi-Fi (see
// docs/wifi_protocol.md and XIAO_OpenVisionEye/AudioManager.h) to
// BluetoothA2DPSource's fixed-format requirement: the ESP32-A2DP library's
// own documentation states A2DP source audio is always 44.1kHz, stereo,
// 16-bit, and that this sample rate "is defined by the ESP32 A2DP and can't
// be changed" (confirmed against the library's GitHub discussions). So this
// class does the mono->stereo duplication and 16kHz->44.1kHz upsampling
// itself before handing samples to the Bluetooth callback.
//
// The upsampling is simple nearest-sample repetition (a fractional-step
// accumulator), not a high-quality resampler — good enough for voice
// prompts and casual music listening, not audiophile-grade. Documented
// rather than hidden, per your requirement.

#pragma once
#include <Arduino.h>

class AudioOutput {
public:
  // ringCapacitySamples: how many mono input samples we can buffer before
  // we start dropping the oldest ones (protects RAM on a small MCU; a
  // dropped buffer means an audible glitch, not a crash).
  bool begin(size_t ringCapacitySamples = 16000); // ~1s of audio at 16kHz

  // Called from WiFiAudio's PCM handler with raw little-endian mono 16-bit
  // PCM bytes (len must be a multiple of 2).
  void pushMonoPcm(const uint8_t* data, size_t len);

  bool hasBufferedAudio() const { return _fill > 0; }
  size_t bufferedSampleCount() const { return _fill; }

  // The single global instance the static A2DP callback wrapper reads from.
  // ESP32-A2DP's set_data_callback takes a plain function pointer (not a
  // std::function / member function), so a global instance pointer is the
  // straightforward, real way to bridge into this class — not a workaround
  // for a missing feature, just how that C-style callback API works.
  static AudioOutput* instance;

  // The actual callback registered with a2dp_source.set_data_callback().
  static int32_t fillA2dpBuffer(uint8_t* data, int32_t byteCount);

private:
  int16_t* _ring = nullptr;
  size_t _capacity = 0;
  size_t _head = 0;   // next write index
  size_t _tail = 0;   // next read index
  size_t _fill = 0;   // samples currently buffered
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

  // Fractional playback-position accumulator for 16kHz -> 44.1kHz.
  float _resamplePhase = 0.0f;
  static constexpr float kStep = 16000.0f / 44100.0f;

  bool popResampledStereoFrame(int16_t& left, int16_t& right);
};
