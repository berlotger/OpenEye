// AudioOutput.cpp
// STATUS: 1 REAL

#include "AudioOutput.h"
#include <string.h>

AudioOutput* AudioOutput::instance = nullptr;

bool AudioOutput::begin(size_t ringCapacitySamples) {
  _ring = (int16_t*)malloc(ringCapacitySamples * sizeof(int16_t));
  if (!_ring) {
    Serial.println("[AudioOutput] failed to allocate ring buffer");
    return false;
  }
  _capacity = ringCapacitySamples;
  _head = _tail = _fill = 0;
  instance = this;
  Serial.printf("[AudioOutput] ring buffer ready (%u samples)\n", (unsigned)_capacity);
  return true;
}

void AudioOutput::pushMonoPcm(const uint8_t* data, size_t len) {
  if (!_ring) return;
  size_t sampleCount = len / 2;
  const int16_t* samples = reinterpret_cast<const int16_t*>(data);

  portENTER_CRITICAL(&_mux);
  for (size_t i = 0; i < sampleCount; i++) {
    _ring[_head] = samples[i];
    _head = (_head + 1) % _capacity;
    if (_fill < _capacity) {
      _fill++;
    } else {
      // Buffer full: drop the oldest sample instead of overrunning — an
      // audible glitch under sustained overload, not a crash.
      _tail = (_tail + 1) % _capacity;
    }
  }
  portEXIT_CRITICAL(&_mux);
}

bool AudioOutput::popResampledStereoFrame(int16_t& left, int16_t& right) {
  static int16_t lastSample = 0;
  static bool havePrimed = false;

  _resamplePhase += kStep;
  if (_resamplePhase >= 1.0f) {
    _resamplePhase -= 1.0f;
    portENTER_CRITICAL(&_mux);
    if (_fill > 0) {
      lastSample = _ring[_tail];
      _tail = (_tail + 1) % _capacity;
      _fill--;
      havePrimed = true;
    }
    portEXIT_CRITICAL(&_mux);
  }

  if (!havePrimed) {
    left = right = 0;
    return false;
  }
  left = right = lastSample;
  return true;
}

int32_t AudioOutput::fillA2dpBuffer(uint8_t* data, int32_t byteCount) {
  if (!instance) {
    memset(data, 0, byteCount);
    return byteCount;
  }
  // byteCount is stereo 16-bit frames: 4 bytes per frame (L+R).
  int32_t frames = byteCount / 4;
  int16_t* out = reinterpret_cast<int16_t*>(data);
  for (int32_t i = 0; i < frames; i++) {
    int16_t l, r;
    instance->popResampledStereoFrame(l, r);
    out[i * 2] = l;
    out[i * 2 + 1] = r;
  }
  return byteCount;
}
