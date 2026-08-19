// BluetoothAudio.h
// STATUS: 2 REAL — needs a library (ESP32-A2DP by pschatzmann). Verified
// against the library's own Doxygen class reference for BluetoothA2DPSource:
// `#include <BluetoothA2DPSource.h>`, `set_data_callback(int32_t(*)(uint8_t*,int32_t))`,
// `start(const char*)`, `set_volume(uint8_t)` (range 0-127), and
// `is_connected()` ("Checks if A2DP is connected using the connection state
// received from the speaker.") — all real, documented public methods, not
// inferred from examples.
//
// Hardware requirement: classic ESP32 only — see docs/hardware.md §2.1.

#pragma once
#include <Arduino.h>
#include <BluetoothA2DPSource.h>
#include "AudioOutput.h"

class BluetoothAudio {
public:
  // headsetName: the exact Bluetooth name of your headphones/earbuds, as
  // shown when pairing them with a phone. If empty, start() falls back to
  // connecting to the first previously-paired/discoverable audio sink it
  // finds (library default discovery behavior) — see installation.md for
  // how to find the exact name if you want to pin it down.
  bool begin(const char* headsetName, AudioOutput* output);

  bool isConnected();
  void setVolume(uint8_t volume0to127);

private:
  BluetoothA2DPSource _a2dp;
};
