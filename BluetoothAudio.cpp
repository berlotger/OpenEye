// BluetoothAudio.cpp
// STATUS: 2 REAL (see header)

#include "BluetoothAudio.h"

bool BluetoothAudio::begin(const char* headsetName, AudioOutput* output) {
  if (!output) return false;
  if (AudioOutput::instance != output) {
    // AudioOutput::begin() sets its own static `instance` pointer (needed
    // because the A2DP library's callback is a plain function pointer, not
    // a std::function — see AudioOutput.h). This check just confirms the
    // caller initialized AudioOutput first; it's not a second registration.
    Serial.println("[BluetoothAudio] warning: AudioOutput::begin() must be called before BluetoothAudio::begin()");
  }

  _a2dp.set_data_callback(AudioOutput::fillA2dpBuffer);
  _a2dp.set_auto_reconnect(true);

  if (headsetName != nullptr && strlen(headsetName) > 0) {
    Serial.printf("[BluetoothAudio] starting A2DP source, target device \"%s\"\n", headsetName);
    _a2dp.start(headsetName);
  } else {
    Serial.println("[BluetoothAudio] starting A2DP source with no target name set — "
                    "will use the library's own discovery/auto-reconnect behavior. "
                    "Set AudioHubConfig::HEADSET_BT_NAME for a deterministic connection.");
    _a2dp.start();
  }
  return true;
}

bool BluetoothAudio::isConnected() {
  return _a2dp.is_connected();
}

void BluetoothAudio::setVolume(uint8_t volume0to127) {
  _a2dp.set_volume(volume0to127);
}
