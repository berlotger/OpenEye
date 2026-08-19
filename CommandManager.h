// CommandManager.h
// STATUS: 1 REAL — the dispatcher plumbing is real and complete. The one gap
// (by design, see docs/architecture.md) is that command *text* comes from
// Serial/TCP input in v1, not from on-device speech-to-text.
//
// Flow: WakeWordEngine::poll() fires a wake word -> CommandManager opens a
// short listening window -> the next text line from Serial or the TCP
// diagnostic port is matched against a small fixed phrase table -> the
// matching action runs.

#pragma once
#include <Arduino.h>
#include <vector>
#include "WakeWordEngine.h"
#include "CameraManager.h"
#include "SDManager.h"
#include "AudioManager.h"
#include "BatteryManager.h"
#include "WiFiManager.h"
#include "ConfigManager.h"
#include "VisionAI.h"
#include "LanguageAI.h"

class CommandManager {
public:
  void begin(WakeWordEngine* wake, CameraManager* camera, SDManager* sd,
             AudioManager* audio, BatteryManager* battery, WiFiManager* wifi,
             ConfigManager* config, VisionAI* vision, LanguageAI* language);

  // Call every loop() iteration.
  void loop();

  // Feed one line of text (from Serial or a TCP client) as a possible
  // command or wake-word injection.
  void handleIncomingText(const String& line);

  // Feed one control-channel line received from the ESP32 Audio board.
  void handleControlLine(const String& line);

private:
  WakeWordEngine* _wake = nullptr;
  CameraManager* _camera = nullptr;
  SDManager* _sd = nullptr;
  AudioManager* _audio = nullptr;
  BatteryManager* _battery = nullptr;
  WiFiManager* _wifi = nullptr;
  ConfigManager* _config = nullptr;
  VisionAI* _vision = nullptr;
  LanguageAI* _language = nullptr;

  bool _listening = false;
  unsigned long _listenUntil = 0;
  WakeWord _pendingWake = WakeWord::NONE;
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
  void dispatchGlassesCommand(const String& text);
  void dispatchAiCommand(const String& text);

  void doTakePhoto();
  void doStartRecording();
  void doStopRecording();
  void doPlayMusic();
  void doPauseMusic();
  void doNextSong();
  void doPreviousSong();
  void doBatteryStatus();
  void say(const String& text); // sends SAY:<text> to the Audio hub + logs it
};
