// CommandManager.h
// STATUS: 1 REAL — the dispatcher plumbing is real and complete. The one gap
// (by design, see docs/architecture.md) is that command *text* comes from
// Serial/TCP input in v1.1.1-button-voice, not from on-device speech-to-text
// (see STTEngine.h for exactly why, and what plugs in later).
//
// ---- PRIMARY flow in this version (per your spec §7/§8: WAKE WORD = FUTURE,
// the button is the real wake trigger) ----
//   Long-press the ButtonManager button (see ButtonManager.h)
//     -> handleButtonAction(ButtonAction::COMMAND_MODE)
//     -> CommandManager opens ONE command-listening window (Stage::GLASSES_COMMAND)
//     -> next text line (typed on Serial/TCP today — a real STT engine would
//        plug in here later, see STTEngine.h) is matched against the fixed
//        phrase table ("take a photo", "start recording", "record audio",
//        "battery", "play music", ...) and dispatched directly.
//   Single-click / double-click on the same button fire PHOTO / VIDEO_TOGGLE
//   immediately, no listening window at all — see handleButtonAction().
//   "HEY_GLASSES" typed directly opens the same single-stage listening
//   window as the button, as a bench-testing shortcut that doesn't need
//   physical hardware wired up yet.
//
// ---- SECONDARY / optional / disabled by default: the "Jarvis" flow ----
// A separate, optional two-stage flow also exists (WakeWordEngine::poll()
// firing WakeWord::JARVIS -> Stage::MODE_SELECT -> a mode word ["glasses"/
// "ai"/"music"] -> Stage::GLASSES_COMMAND or Stage::AI_COMMAND). This is the
// v1.0/v1.1 "Jarvis" wake-word stub (see WakeWordEngine.h) — it is real,
// working code, kept for anyone who wants it, but per your spec it is NOT
// the primary flow this version and its automatic trigger (the BOOT button)
// is now gated off by default via ConfigManager::wakeWordEnabled — see
// onWake()/loop() and ConfigManager.h. "JARVIS"/"HEY_JARVIS"/"HEY_AI" typed
// over Serial/TCP still work as manual bench-testing shortcuts either way.

#pragma once
#include <Arduino.h>
#include <vector>
#include "WakeWordEngine.h"
#include "ButtonManager.h"
#include "CameraManager.h"
#include "SDManager.h"
#include "AudioManager.h"
#include "AudioBackend.h"
#include "BatteryManager.h"
#include "WiFiManager.h"
#include "ConfigManager.h"
#include "VisionAI.h"
#include "LanguageAI.h"
#include "VoiceCommandIds.h"

class CommandManager {
public:
  void begin(WakeWordEngine* wake, CameraManager* camera, SDManager* sd,
             AudioManager* audio, AudioBackendManager* audioBackend,
             BatteryManager* battery, WiFiManager* wifi,
             ConfigManager* config, VisionAI* vision, LanguageAI* language,
             ButtonManager* button);

  // Call every loop() iteration.
  void loop();

  // Feed one line of text (from Serial or a TCP client) as a possible
  // command, mode word, or wake-word injection.
  void handleIncomingText(const String& line);

  // Feed one control-channel line received from the ESP32 Audio board.
  void handleControlLine(const String& line);

  // Feed a click detected by ButtonManager (see XIAO_OpenVisionEye.ino).
  void handleButtonAction(ButtonAction action);

  // v1.2 AI Offline: feed a command recognized by MultiNetSTT (only ever
  // called from the main .ino when VOICE_MODE=MULTINET — see
  // MultiNetSTT.h for why that's off by default). Dispatches directly to
  // the same doXxx() methods the button/text paths use — no separate
  // logic path to keep in sync.
  void handleVoiceCommand(VoiceCommandId id);

private:
  WakeWordEngine* _wake = nullptr;
  CameraManager* _camera = nullptr;
  SDManager* _sd = nullptr;
  AudioManager* _audio = nullptr;
  AudioBackendManager* _audioBackend = nullptr;
  BatteryManager* _battery = nullptr;
  WiFiManager* _wifi = nullptr;
  ConfigManager* _config = nullptr;
  VisionAI* _vision = nullptr;
  LanguageAI* _language = nullptr;
  ButtonManager* _button = nullptr;

  // ---- Two-stage Jarvis flow state ----
  enum class Stage {
    IDLE,
    MODE_SELECT,     // heard "Jarvis", waiting for a mode word
    GLASSES_COMMAND, // heard "Jarvis"+"glasses" (or the HEY_GLASSES shortcut), waiting for a system command
    AI_COMMAND,      // heard "Jarvis"+"ai" (or the HEY_AI shortcut), waiting for a question
  };
  Stage _stage = Stage::IDLE;
  unsigned long _stageDeadline = 0;
  static constexpr unsigned long MODE_SELECT_WINDOW_MS = 6000;  // "5-8s" from the spec
  static constexpr unsigned long LISTEN_WINDOW_MS = 8000;

  bool _recording = false;
  String _currentVideoPath;
  unsigned long _videoStartedAt = 0;
  static constexpr unsigned long MAX_VIDEO_MS = 15000; // keep v1 clips short/stable

  std::vector<String> _playlist;
  int _playlistIndex = -1;
  void refreshPlaylist();
  void playCurrentTrack(); // blocking for the duration of the file — see doPlayMusic()

  void onWake(WakeWord w);
  void onModeWord(const String& text);
  void dispatchGlassesCommand(const String& text);
  void dispatchAiCommand(const String& text);
  bool handleButtonConfigCommand(const String& line); // BUTTON_SINGLE:/BUTTON_DOUBLE:/BUTTON_LONG:
  bool handleModeConfigCommand(const String& line);    // VISION_MODE:/VOICE_MODE:
  void doVisionQuestion(); // shared by "what do you see"/ANALYZE text and voice commands

  void doTakePhoto();
  void doStartRecording();
  void doStopRecording();
  void doRecordAudio();  // "record audio" -> fixed-duration WAV to /OpenVisionEye/audio/ (see .cpp — blocking, documented limitation)
  void doPlayMusic();
  void doPauseMusic();
  void doNextSong();
  void doPreviousSong();
  void doBatteryStatus();
  void doSetVolume(int v); // "volume <0-100>" -> ConfigManager + VOLUME:<n> to the Audio hub
  void doAudioTest();  // AUDIO_TEST diagnostic: 440Hz/1s tone over the full pipeline
  void doPingTest();   // PING_TEST diagnostic: round-trip time to the Audio hub
  void say(const String& text); // sends SAY:<text> to the Audio hub + logs it

  static constexpr uint32_t RECORD_AUDIO_MS = 5000; // fixed v1.1.1 duration, see doRecordAudio()

  bool _pingPending = false;
  unsigned long _pingSentAt = 0;
};
