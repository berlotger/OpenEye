// AudioBackend.h
// STATUS: mixed —
//   AudioBackend interface:    1 REAL
//   LocalAudioBackend:         1 REAL (this is exactly what v1.0 always did —
//                               XIAO -> Wi-Fi -> ESP32 Classic Audio hub -> A2DP)
//   PhoneAudioBackend:         5 FUTURE / NOT IMPLEMENTED (see below — this
//                               is an honest placeholder, not a working path)
//
// Per your requirement: this project should be able to send outgoing audio
// (music, "SAY:" prompts, the AUDIO_TEST tone) through one of two possible
// backends without CommandManager caring which one is active:
//
//   LOCAL  — always the existing ESP32 Classic Audio hub over Wi-Fi -> A2DP.
//   PHONE  — a future companion phone app would receive the PCM instead and
//            play it through the phone's own audio stack (Bluetooth
//            headphones already paired to the phone, phone TTS, etc.).
//   AUTO   — prefer PHONE if it's available, otherwise fall back to LOCAL.
//            Must also fall back to LOCAL automatically mid-session if the
//            phone disappears — see AudioBackendManager::active() below.
//
// IMPORTANT — what is real here and what isn't:
// There is currently NO protocol, pairing mechanism, companion app, or any
// other implementation for getting PCM audio to a phone. Nothing in this
// project talks Bluetooth to a phone, and nothing listens for one on the
// Wi-Fi side either. PhoneAudioBackend below is a stub that always reports
// itself unavailable and always fails to send — it exists purely so the
// AUDIO_MODE selection logic (AUTO/LOCAL/PHONE) and the config.json field
// have somewhere real to point once phone audio is actually built. Do not
// wire a UI or voice command that claims "phone audio" works — it doesn't.

#pragma once
#include <Arduino.h>

class AudioBackend {
public:
  virtual ~AudioBackend() = default;
  virtual bool begin() = 0;
  virtual bool isAvailable() const = 0;
  // Sends one chunk of mono 16-bit 16kHz PCM (same format as everywhere
  // else in this project — see docs/wifi_protocol.md). Returns false if
  // the backend isn't currently able to accept it.
  virtual bool sendPcm(const uint8_t* data, size_t len) = 0;
  virtual const char* name() const = 0;
};

// ---- LOCAL: the only backend that actually does anything in v1.1 ----
// Thin wrapper around WiFiManager::sendAudioChunk() — same real behavior
// v1.0 always had, just reachable through the AudioBackend interface now.
class WiFiManager; // fwd decl, avoids a circular include

class LocalAudioBackend : public AudioBackend {
public:
  explicit LocalAudioBackend(WiFiManager* wifi) : _wifi(wifi) {}
  bool begin() override { return true; }
  bool isAvailable() const override;
  bool sendPcm(const uint8_t* data, size_t len) override;
  const char* name() const override { return "LOCAL"; }

private:
  WiFiManager* _wifi;
};

// ---- PHONE: NOT IMPLEMENTED — see the big header comment above ----
class PhoneAudioBackend : public AudioBackend {
public:
  bool begin() override { return true; } // nothing to initialize — there is nothing here yet
  bool isAvailable() const override { return false; } // always false — see header comment
  bool sendPcm(const uint8_t* /*data*/, size_t /*len*/) override {
    if (!_warned) {
      Serial.println("[PhoneAudioBackend] NOT IMPLEMENTED — no phone audio path exists yet. "
                      "See AudioBackend.h. Falling back to LOCAL should happen in AudioBackendManager, "
                      "not here — if you're seeing this warning repeatedly, check that fallback logic.");
      _warned = true;
    }
    return false;
  }
  const char* name() const override { return "PHONE (not implemented)"; }

private:
  bool _warned = false;
};

// Resolves AUDIO_MODE (AUTO/LOCAL/PHONE, from config.json) to the backend
// that should actually be used for the *next* chunk. Deliberately
// re-resolves on every call rather than caching a choice, so AUTO can fall
// back from PHONE to LOCAL mid-session if the phone disappears, per your
// requirement — and, symmetrically, could pick PHONE back up automatically
// once that backend is real and isAvailable() actually means something.
class AudioBackendManager {
public:
  enum class Mode { AUTO, LOCAL, PHONE };

  AudioBackendManager(LocalAudioBackend* local, PhoneAudioBackend* phone)
    : _local(local), _phone(phone) {}

  void setMode(Mode m) { _mode = m; }
  static Mode modeFromString(const String& s);

  // Returns the backend to use right now. Never returns nullptr — falls
  // back to LOCAL whenever PHONE isn't available, in both AUTO and PHONE
  // modes (PHONE mode requesting a backend that doesn't exist yet would
  // otherwise silently drop audio; falling back to LOCAL with a one-time
  // warning is the safer default).
  AudioBackend* active();

  bool sendPcm(const uint8_t* data, size_t len) { return active()->sendPcm(data, len); }

private:
  LocalAudioBackend* _local;
  PhoneAudioBackend* _phone;
  Mode _mode = Mode::AUTO;
  bool _warnedPhoneModeFallback = false;
};
