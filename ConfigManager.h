// ConfigManager.h
// STATUS: 1 REAL — needs the ArduinoJson library (see requirements.md).
//
// Reads/writes /OpenVisionEye/config/config.json on the SD card. No Wi-Fi
// internet credentials are ever stored here — the XIAO's own SoftAP doesn't
// need a password field for a client network, and this project has no
// internet mode in v1 (see docs/wifi_protocol.md and README "Security").

#pragma once
#include <Arduino.h>

struct OpenVisionEyeConfig {
  String language = "en";              // "en" (recommended for v1), "ca", "es"
  int volume = 70;                     // 0-100
  // v1.1-button-voice: OFF by default. The button (ButtonManager, see
  // ButtonManager.h) is now the primary/documented command trigger — it
  // does not depend on this flag at all. This flag only gates the
  // separate, optional "Jarvis" manual-trigger stub (BOOT button /
  // Serial-TCP "JARVIS" text — see WakeWordEngine.h). Per your spec:
  // WAKE WORD = FUTURE in this version, so it ships disabled; set true in
  // config.json if you still want the BOOT-button "Jarvis" path alongside
  // the button. The JARVIS/HEY_GLASSES/HEY_AI *text* shortcuts still work
  // over Serial/TCP either way, for bench testing — see CommandManager.cpp.
  bool wakeWordEnabled = false;
  String audioMode = "wifi_to_bt";     // only mode implemented in v1
  bool batteryAdcDividerEnabled = false; // see BatteryManager.h — off unless you wired it
  int batteryAdcPin = -1;              // -1 = use the board's own A0 macro (recommended).
                                        // Only override with a raw GPIO number if you've
                                        // confirmed it on your specific board revision —
                                        // community sources disagree on the exact GPIO
                                        // number behind "A0" across XIAO ESP32-S3 batches.
  String apSsidPrefix = "OpenVisionEye";
  // Local Wi-Fi AP password only (device-to-device, never leaves the two
  // boards). Not an "internet credential" — see README.md "Security".
  // Change this in config.json before relying on it for anything sensitive.
  String apPassword = "glasses1234";

  // v1.1: ButtonManager mapping (see ButtonManager.h). Values are one of
  // "PHOTO", "VIDEO_TOGGLE", "COMMAND_MODE", "NONE". Changeable at runtime
  // via the BUTTON_SINGLE:/BUTTON_DOUBLE:/BUTTON_LONG: text commands (see
  // CommandManager.cpp) without reflashing — the new mapping is persisted
  // here.
  String buttonSingleAction = "PHOTO";
  String buttonDoubleAction = "VIDEO_TOGGLE";
  String buttonLongAction   = "COMMAND_MODE";

  // v1.1: AudioBackend selection (see AudioBackend.h). One of "AUTO",
  // "LOCAL", "PHONE". PHONE is NOT IMPLEMENTED in v1.1 — see
  // PhoneAudioBackend. AUTO behaves identically to LOCAL today because
  // phone audio never becomes available; the field exists so the
  // architecture doesn't need to change again once phone audio is real.
  String audioBackendMode = "AUTO";

  // v1.2 AI Offline: VISION_MODE is one of "OFF" (default — NullVisionAI,
  // 1 REAL) or "LOCAL" (EdgeImpulseVisionAI, 4 EXPERIMENTAL — requires you
  // to have added your own exported Edge Impulse library AND compiled with
  // OVE_ENABLE_EDGE_IMPULSE defined; see docs/installation.md). Defaults to
  // OFF because that combination is not something this project can ship
  // pre-verified — see VisionAI.h.
  String visionMode = "OFF";

  // VOICE_MODE is one of "OFF" (default — the button/HEY_GLASSES stay the
  // only way to open a command-listening window) or "MULTINET" (4
  // EXPERIMENTAL — requires OVE_ENABLE_MULTINET at compile time AND a
  // partition scheme with a MultiNet model partition; NOT confirmed to
  // boot on the 8 MB XIAO ESP32S3 Sense — see MultiNetSTT.h). Defaults to
  // OFF for the same reason as visionMode above.
  String voiceMode = "OFF";
};

class ConfigManager {
public:
  bool begin();  // loads from SD, or writes+loads defaults if missing
  const OpenVisionEyeConfig& get() const { return _config; }
  bool save();

  // Mutators used by CommandManager when a setting changes at runtime
  // (e.g. VOLUME:<n> from the Wi-Fi protocol).
  void setVolume(int v);
  void setButtonMapping(const String& single, const String& dbl, const String& longPress);
  void setVisionMode(const String& mode);
  void setVoiceMode(const String& mode);

private:
  OpenVisionEyeConfig _config;
  static constexpr const char* kPath = "/OpenVisionEye/config/config.json";
  bool loadFromDisk();
  bool writeDefaults();
};
