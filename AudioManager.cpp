// AudioManager.cpp
// STATUS: see header. Mic capture uses ESP_I2S.h's documented per-sample
// I2S.read() API (this library version does not document a batch/blocking
// readBytes call, so we don't assume one exists — see Seeed's own mic
// tutorial in docs/hardware.md §1.3 for the same pattern).

#include "AudioManager.h"

bool AudioManager::begin(SDManager* sd) {
  _sd = sd;
  _i2s.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  _i2sReady = _i2s.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  if (!_i2sReady) {
    Serial.println("[AudioManager] mic init failed");
    return false;
  }
  Serial.println("[AudioManager] mic ready (PDM, 16kHz mono)");
  return true;
}

bool AudioManager::streamMicTo(PcmSinkFn sink, uint32_t durationMs) {
  if (_source != AudioInputSource::MIC_XIAO) {
    Serial.println("[AudioManager] streamMicTo: only MIC_XIAO is implemented in v1");
    return false;
  }
  if (!_i2sReady) return false;

  const size_t CHUNK_SAMPLES = 512;
  int16_t buf[CHUNK_SAMPLES];
  unsigned long start = millis();

  while (millis() - start < durationMs) {
    size_t n = 0;
    while (n < CHUNK_SAMPLES) {
      int sample = _i2s.read();
      if (sample != -1 && sample != 1) { // library's own "no data yet" sentinel values
        buf[n++] = (int16_t)sample;
      }
    }
    sink(reinterpret_cast<const uint8_t*>(buf), n * sizeof(int16_t));
  }
  return true;
}

bool AudioManager::writeWavHeader(File& f, uint32_t dataBytes) {
  // Minimal canonical 44-byte PCM WAV header, mono 16-bit @ SAMPLE_RATE.
  uint32_t byteRate = SAMPLE_RATE * 1 * 16 / 8;
  uint16_t blockAlign = 1 * 16 / 8;
  uint32_t chunkSize = 36 + dataBytes;

  f.seek(0);
  f.write((const uint8_t*)"RIFF", 4);
  f.write((const uint8_t*)&chunkSize, 4);
  f.write((const uint8_t*)"WAVE", 4);
  f.write((const uint8_t*)"fmt ", 4);
  uint32_t subchunk1Size = 16;
  f.write((const uint8_t*)&subchunk1Size, 4);
  uint16_t audioFormat = 1; // PCM
  f.write((const uint8_t*)&audioFormat, 2);
  uint16_t numChannels = 1;
  f.write((const uint8_t*)&numChannels, 2);
  uint32_t sampleRate = SAMPLE_RATE;
  f.write((const uint8_t*)&sampleRate, 4);
  f.write((const uint8_t*)&byteRate, 4);
  f.write((const uint8_t*)&blockAlign, 2);
  uint16_t bitsPerSample = 16;
  f.write((const uint8_t*)&bitsPerSample, 2);
  f.write((const uint8_t*)"data", 4);
  f.write((const uint8_t*)&dataBytes, 4);
  return true;
}

bool AudioManager::recordToFile(const String& path, uint32_t durationMs) {
  if (!_sd || !_sd->isReady() || !_i2sReady) return false;

  File f = _sd->openForWrite(path.c_str());
  if (!f) {
    Serial.printf("[AudioManager] could not open %s for recording\n", path.c_str());
    return false;
  }

  // Reserve space for the header, fill it with real values once we know
  // dataBytes.
  uint8_t placeholder[44] = {0};
  f.write(placeholder, 44);

  uint32_t dataBytes = 0;
  const size_t CHUNK_SAMPLES = 512;
  int16_t buf[CHUNK_SAMPLES];
  unsigned long start = millis();

  while (millis() - start < durationMs) {
    size_t n = 0;
    while (n < CHUNK_SAMPLES) {
      int sample = _i2s.read();
      if (sample != -1 && sample != 1) {
        buf[n++] = (int16_t)sample;
      }
    }
    size_t bytes = n * sizeof(int16_t);
    f.write(reinterpret_cast<const uint8_t*>(buf), bytes);
    dataBytes += bytes;
  }

  writeWavHeader(f, dataBytes);
  f.close();
  Serial.printf("[AudioManager] recorded %u bytes to %s\n", (unsigned)dataBytes, path.c_str());
  return true;
}

bool AudioManager::streamFileTo(const String& path, PcmSinkFn sink) {
  if (!_sd || !_sd->isReady()) return false;
  File f = _sd->openForRead(path.c_str());
  if (!f) {
    Serial.printf("[AudioManager] file not found: %s\n", path.c_str());
    return false;
  }

  // Expect a canonical 44-byte PCM WAV header; skip it rather than parse
  // every chunk type, since v1 only ever writes/consumes its own files.
  if (f.size() < 44) {
    Serial.printf("[AudioManager] %s too small to be a WAV file\n", path.c_str());
    f.close();
    return false;
  }
  f.seek(44);

  // Approximate real-time pacing so the receiver's buffer doesn't have to
  // absorb the whole file at Wi-Fi speed. Not sample-accurate, but close
  // enough for A2DP's own internal buffering to smooth out — see
  // docs/architecture.md. bytesPerMs assumes mono 16-bit @ SAMPLE_RATE.
  const float bytesPerMs = (SAMPLE_RATE * 2) / 1000.0f;
  const size_t BUF_BYTES = 1024;
  uint8_t buf[BUF_BYTES];
  while (f.available()) {
    unsigned long chunkStart = millis();
    size_t n = f.read(buf, BUF_BYTES);
    if (n == 0) break;
    sink(buf, n);
    unsigned long targetMs = (unsigned long)(n / bytesPerMs);
    unsigned long elapsed = millis() - chunkStart;
    if (targetMs > elapsed) delay(targetMs - elapsed);
  }
  f.close();
  return true;
}
