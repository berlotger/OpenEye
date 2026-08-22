// ESP32_Audio_A2DP_only.ino
// STATUS: 2 REAL (needs the ESP32-A2DP library) — minimal isolation test.
//
// Purpose: verify Bluetooth Classic A2DP + your specific headphones work at
// all, BEFORE trying the full OpenVisionEye Wi-Fi+Bluetooth pipeline. If
// this doesn't compile, you have selected the wrong board — see
// docs/hardware.md §2.1. If it compiles but never connects, see
// docs/troubleshooting.md "Compiles but Bluetooth pairing never happens".
//
// Board: any CLASSIC ESP32 (e.g. "ESP32 Dev Module") — NOT S2/S3/C3/C6.
//
// Behavior: generates a continuous 440Hz test tone and streams it to your
// headphones by name. Set HEADSET_NAME below before uploading.

#include <BluetoothA2DPSource.h>
#include <math.h>

#define HEADSET_NAME "YOUR_HEADPHONES_NAME_HERE"

BluetoothA2DPSource a2dp_source;

// Generates a simple 440Hz sine tone as stereo 16-bit PCM — used only to
// prove the Bluetooth audio path works, independent of any Wi-Fi/SD logic.
int32_t get_test_tone(uint8_t* data, int32_t byteCount) {
  static float phase = 0.0f;
  const float freq = 440.0f;
  const float sampleRate = 44100.0f;
  int16_t* samples = reinterpret_cast<int16_t*>(data);
  int32_t frames = byteCount / 4; // stereo 16-bit = 4 bytes/frame

  for (int32_t i = 0; i < frames; i++) {
    int16_t v = (int16_t)(3000.0f * sinf(phase));
    samples[i * 2] = v;     // left
    samples[i * 2 + 1] = v; // right
    phase += 2.0f * PI * freq / sampleRate;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
  }
  return byteCount;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 A2DP-only test — connecting to " HEADSET_NAME);

  a2dp_source.set_data_callback(get_test_tone);
  a2dp_source.set_volume(60);
  a2dp_source.start(HEADSET_NAME);
}

void loop() {
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 3000) {
    lastLog = millis();
    Serial.printf("connected: %s\n", a2dp_source.is_connected() ? "yes" : "no");
  }
  delay(10);
}
