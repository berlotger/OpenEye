// AudioBackend.cpp
// STATUS: see AudioBackend.h

#include "AudioBackend.h"
#include "WiFiManager.h"

bool LocalAudioBackend::isAvailable() const {
  return _wifi && _wifi->audioClientConnected();
}

bool LocalAudioBackend::sendPcm(const uint8_t* data, size_t len) {
  if (!_wifi) return false;
  return _wifi->sendAudioChunk(data, len);
}

AudioBackendManager::Mode AudioBackendManager::modeFromString(const String& sIn) {
  String s = sIn; s.toUpperCase(); s.trim();
  if (s == "LOCAL") return Mode::LOCAL;
  if (s == "PHONE") return Mode::PHONE;
  return Mode::AUTO; // default, and the safe fallback for an unrecognized value
}

AudioBackend* AudioBackendManager::active() {
  switch (_mode) {
    case Mode::LOCAL:
      return _local;

    case Mode::PHONE:
      if (_phone->isAvailable()) return _phone;
      if (!_warnedPhoneModeFallback) {
        Serial.println("[AudioBackendManager] AUDIO_MODE=PHONE but PhoneAudioBackend is not "
                        "implemented (see AudioBackend.h) — using LOCAL instead.");
        _warnedPhoneModeFallback = true;
      }
      return _local;

    case Mode::AUTO:
    default:
      // Prefer phone if it's ever actually available; today it never is,
      // so this always resolves to LOCAL. No warning here — AUTO
      // preferring LOCAL when there's no phone is expected, not an error.
      return _phone->isAvailable() ? _phone : _local;
  }
}
