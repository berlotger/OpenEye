// CommandManager.cpp
// STATUS: 1 REAL (see header for the one honest gap: command text source)

#include "CommandManager.h"
#include <string.h>

static bool containsIgnoreCase(const String& haystack, const char* needle) {
  String h = haystack; h.toLowerCase();
  String n = needle; n.toLowerCase();
  return h.indexOf(n) >= 0;
}

void CommandManager::begin(WakeWordEngine* wake, CameraManager* camera, SDManager* sd,
                            AudioManager* audio, AudioBackendManager* audioBackend,
                            BatteryManager* battery, WiFiManager* wifi,
                            ConfigManager* config, VisionAI* vision, LanguageAI* language,
                            ButtonManager* button) {
  _wake = wake; _camera = camera; _sd = sd; _audio = audio; _audioBackend = audioBackend;
  _battery = battery; _wifi = wifi; _config = config; _vision = vision;
  _language = language; _button = button;
}

void CommandManager::loop() {
  // Only the BOOT-button/auto-trigger side of WakeWordEngine is gated by
  // this flag (default false — see ConfigManager.h). The "JARVIS"/
  // "HEY_JARVIS" *text* shortcuts in handleIncomingText() still work
  // either way, since those are an explicit manual action, not an
  // always-on listener. See CommandManager.h for why the button, not
  // Jarvis, is the primary trigger in this version.
  if (_wake && _config && _config->get().wakeWordEnabled) {
    WakeWord w = _wake->poll();
    if (w != WakeWord::NONE) onWake(w);
  }

  if (_stage != Stage::IDLE && millis() > _stageDeadline) {
    Stage closing = _stage;
    _stage = Stage::IDLE;
    if (closing == Stage::MODE_SELECT) {
      Serial.println("[CommandManager] mode window closed (no mode word received)");
    } else {
      Serial.println("[CommandManager] listening window closed (no command received)");
    }
  }

  // Ongoing video recording: append one frame per loop() call, bounded by
  // MAX_VIDEO_MS. This is the same "best-effort MJPEG" technique used in
  // Seeed's own tutorials — see docs/architecture.md and CameraManager.h.
  if (_recording) {
    if (millis() - _videoStartedAt > MAX_VIDEO_MS) {
      doStopRecording();
    } else {
      camera_fb_t* fb = _camera->capture();
      if (fb) {
        uint32_t lenLE = fb->len;
        _sd->appendFile(_currentVideoPath.c_str(), reinterpret_cast<uint8_t*>(&lenLE), 4);
        _sd->appendFile(_currentVideoPath.c_str(), fb->buf, fb->len);
        esp_camera_fb_return(fb);
      }
    }
  }
}

void CommandManager::onWake(WakeWord w) {
  if (w != WakeWord::JARVIS) return; // JARVIS is the only wake word in v1.1 — see WakeWordEngine.h
  _stage = Stage::MODE_SELECT;
  _stageDeadline = millis() + MODE_SELECT_WINDOW_MS;
  Serial.println("[CommandManager] \"Jarvis\" heard — listening for a mode (glasses / ai / music)...");
}

void CommandManager::onModeWord(const String& textIn) {
  String text = textIn; text.trim(); text.toLowerCase();
  if (text == "glasses") {
    _stage = Stage::GLASSES_COMMAND;
    _stageDeadline = millis() + LISTEN_WINDOW_MS;
    Serial.println("[CommandManager] mode: glasses — listening for a system command...");
  } else if (text == "ai") {
    _stage = Stage::AI_COMMAND;
    _stageDeadline = millis() + LISTEN_WINDOW_MS;
    Serial.println("[CommandManager] mode: ai — listening for a question...");
  } else if (text == "music") {
    // Reserved mode word per your spec (§7/§27) — not implemented yet.
    // Music playback itself exists (doPlayMusic() etc.) but there is no
    // dedicated MUSIC_MODE command grammar built on top of it yet; the
    // existing "play music"/"next"/etc. phrases only work inside
    // GLASSES_COMMAND today. Extending this is a small, contained
    // follow-up once you decide the exact phrase table for MUSIC mode.
    _stage = Stage::IDLE;
    Serial.println("[CommandManager] mode \"music\" is reserved but not implemented yet.");
    say("Music mode is not available yet.");
  } else {
    _stage = Stage::IDLE;
    Serial.println("[CommandManager] unrecognized mode word: " + text);
    say("I didn't recognize that mode.");
  }
}

void CommandManager::handleIncomingText(const String& lineIn) {
  String line = lineIn; line.trim();
  if (line.length() == 0) return;

  // Allow text-injected wake word (useful before real wake-word detection
  // exists — see WakeWordEngine.h).
  if (line.equalsIgnoreCase("JARVIS") || line.equalsIgnoreCase("HEY_JARVIS")) {
    onWake(WakeWord::JARVIS);
    return;
  }

  // TEST shortcuts: skip straight to a command-listening window without
  // going through Jarvis + a mode word. See CommandManager.h flow comment.
  if (line.equalsIgnoreCase("HEY_GLASSES")) {
    // Bench-testing stand-in for a long-press on the physical button (the
    // PRIMARY trigger in this version) when you don't have it wired yet.
    _stage = Stage::GLASSES_COMMAND;
    _stageDeadline = millis() + LISTEN_WINDOW_MS;
    Serial.println("[CommandManager] (shortcut) command mode — listening for a command...");
    say("Listening.");
    return;
  }
  if (line.equalsIgnoreCase("HEY_AI")) {
    _stage = Stage::AI_COMMAND;
    _stageDeadline = millis() + LISTEN_WINDOW_MS;
    Serial.println("[CommandManager] (shortcut) mode: ai — listening for a question...");
    return;
  }

  // Diagnostic commands: available any time, no wake word needed, from
  // Serial or (via the fallback in handleControlLine()) the TCP control
  // channel too — see docs/wifi_protocol.md "AUDIO_TEST" / "PING_TEST".
  if (line.equalsIgnoreCase("AUDIO_TEST")) { doAudioTest(); return; }
  if (line.equalsIgnoreCase("PING_TEST"))  { doPingTest(); return; }

  // Button remap commands: also available any time, no wake word needed —
  // see docs/wifi_protocol.md and ButtonManager.h.
  if (handleButtonConfigCommand(line)) return;

  // VISION_MODE:/VOICE_MODE: — also available any time. See
  // ConfigManager.h for why both default to OFF and what MULTINET/LOCAL
  // actually require before they'll do anything.
  if (handleModeConfigCommand(line)) return;

  if (_stage == Stage::MODE_SELECT) {
    onModeWord(line);
    return;
  }

  if (_stage == Stage::GLASSES_COMMAND) {
    _stage = Stage::IDLE;
    dispatchGlassesCommand(line);
    return;
  }

  if (_stage == Stage::AI_COMMAND) {
    _stage = Stage::IDLE;
    dispatchAiCommand(line);
    return;
  }

  Serial.println("[CommandManager] got text but nothing is listening — ignoring: " + line);
}

bool CommandManager::handleButtonConfigCommand(const String& line) {
  // Format: BUTTON_SINGLE:<ACTION>, BUTTON_DOUBLE:<ACTION>, BUTTON_LONG:<ACTION>
  // <ACTION> is one of PHOTO, VIDEO_TOGGLE, COMMAND_MODE, NONE (see
  // ButtonManager.h). Persisted to config.json so it survives a reboot.
  // This is what lets you say (type, in v1.1 — see STTEngine.h for why
  // this isn't voice yet) "assign single button to video" style changes
  // without reflashing, per your spec §12.
  const char* prefixes[3] = {"BUTTON_SINGLE:", "BUTTON_DOUBLE:", "BUTTON_LONG:"};
  for (int i = 0; i < 3; i++) {
    if (line.startsWith(prefixes[i])) {
      if (!_button || !_config) return true; // consumed, but nothing to apply to
      String value = line.substring(strlen(prefixes[i]));
      ButtonAction action = buttonActionFromString(value);
      ButtonAction single = _button->singleAction();
      ButtonAction dbl = _button->doubleAction();
      ButtonAction longPress = _button->longAction();
      if (i == 0) single = action;
      else if (i == 1) dbl = action;
      else longPress = action;
      _button->setMapping(single, dbl, longPress);
      _config->setButtonMapping(buttonActionToString(single), buttonActionToString(dbl), buttonActionToString(longPress));
      Serial.printf("[CommandManager] button mapping updated: single=%s double=%s long=%s\n",
        buttonActionToString(single).c_str(), buttonActionToString(dbl).c_str(), buttonActionToString(longPress).c_str());
      say("Button updated.");
      return true;
    }
  }
  return false;
}

bool CommandManager::handleModeConfigCommand(const String& line) {
  // Format: VISION_MODE:<OFF|LOCAL>, VOICE_MODE:<OFF|MULTINET>. Persisted
  // to config.json. Setting a mode to something other than OFF does NOT,
  // by itself, make the underlying engine real — see ConfigManager.h,
  // VisionAI.h, and MultiNetSTT.h. Applying VOICE_MODE takes effect after
  // a reboot (the .ino only constructs/begin()s MultiNetSTT once, at
  // startup) — logged here rather than silently doing nothing.
  if (line.startsWith("VISION_MODE:")) {
    if (!_config) return true;
    String value = line.substring(strlen("VISION_MODE:"));
    value.trim(); value.toUpperCase();
    if (value == "OFF" || value == "LOCAL") {
      _config->setVisionMode(value);
      Serial.println("[CommandManager] visionMode set to " + value + " (LOCAL requires OVE_ENABLE_EDGE_IMPULSE + your own trained model — see docs/installation.md; NullVisionAI stays active either way in this build unless the firmware was recompiled with that flag)");
      say("Vision mode set to " + value + ".");
    } else {
      say("Vision mode must be OFF or LOCAL.");
    }
    return true;
  }
  if (line.startsWith("VOICE_MODE:")) {
    if (!_config) return true;
    String value = line.substring(strlen("VOICE_MODE:"));
    value.trim(); value.toUpperCase();
    if (value == "OFF" || value == "MULTINET") {
      _config->setVoiceMode(value);
      Serial.println("[CommandManager] voiceMode set to " + value + " (takes effect after reboot; MULTINET requires OVE_ENABLE_MULTINET + a partition scheme with a model partition — NOT confirmed to boot on this board, see MultiNetSTT.h)");
      say("Voice mode set to " + value + ". Reboot to apply.");
    } else {
      say("Voice mode must be OFF or MULTINET.");
    }
    return true;
  }
  return false;
}

void CommandManager::handleButtonAction(ButtonAction action) {
  switch (action) {
    case ButtonAction::PHOTO:
      doTakePhoto();
      break;
    case ButtonAction::VIDEO_TOGGLE:
      if (_recording) doStopRecording(); else doStartRecording();
      break;
    case ButtonAction::COMMAND_MODE:
      // PRIMARY flow in this version (spec §7/§8): long-press is the wake
      // trigger substitute — it opens ONE command-listening window
      // directly, no "Jarvis" + mode word needed. ("HEY_GLASSES" typed on
      // Serial/TCP opens this same window, as a bench-testing stand-in for
      // not having the button wired yet — see CommandManager.h.)
      _stage = Stage::GLASSES_COMMAND;
      _stageDeadline = millis() + LISTEN_WINDOW_MS;
      Serial.println("[CommandManager] (button) command mode — listening for a command...");
      say("Listening.");
      break;
    case ButtonAction::NONE:
    default:
      break;
  }
}

void CommandManager::dispatchGlassesCommand(const String& text) {
  if (containsIgnoreCase(text, "take a photo") || containsIgnoreCase(text, "photo")) {
    doTakePhoto();
  } else if (containsIgnoreCase(text, "start recording")) {
    doStartRecording();
  } else if (containsIgnoreCase(text, "stop recording")) {
    doStopRecording();
  } else if (containsIgnoreCase(text, "record audio")) {
    doRecordAudio();
  } else if (containsIgnoreCase(text, "volume")) {
    // Accepts "volume 60", "volume: 60", "set volume 60" — pulls out the
    // first run of digits found anywhere after the word "volume".
    int i = text.indexOf("volume") + 6;
    while (i < (int)text.length() && !isDigit(text[i])) i++;
    int j = i;
    while (j < (int)text.length() && isDigit(text[j])) j++;
    if (j > i) {
      doSetVolume(text.substring(i, j).toInt());
    } else {
      say("Say a volume between 0 and 100.");
    }
  } else if (containsIgnoreCase(text, "play music") || text.equalsIgnoreCase("play")) {
    doPlayMusic();
  } else if (containsIgnoreCase(text, "pause music") || text.equalsIgnoreCase("pause")) {
    doPauseMusic();
  } else if (containsIgnoreCase(text, "next song") || text.equalsIgnoreCase("next")) {
    doNextSong();
  } else if (containsIgnoreCase(text, "previous song") || text.equalsIgnoreCase("previous")) {
    doPreviousSong();
  } else if (containsIgnoreCase(text, "battery")) {
    doBatteryStatus();
  } else {
    Serial.println("[CommandManager] unrecognized system command: " + text);
    say("Sorry, I don't know that command yet.");
  }
}

void CommandManager::dispatchAiCommand(const String& text) {
  if (containsIgnoreCase(text, "what do i see") || containsIgnoreCase(text, "what do you see")
      || containsIgnoreCase(text, "analyze")) {
    doVisionQuestion();
  } else {
    Serial.println("[CommandManager] unrecognized AI question: " + text);
    say("I don't have an answer for that yet.");
  }
}

void CommandManager::doVisionQuestion() {
  camera_fb_t* fb = _camera->capture();
  if (!fb) { say("I couldn't get a frame from the camera."); return; }
  VisionResult r = _vision->analyze(fb);
  esp_camera_fb_return(fb);
  if (!r.available) {
    say(r.statusMessage.length() ? r.statusMessage : "Vision AI is not available yet.");
  } else {
    say(_language->describe(r));
  }
}

void CommandManager::handleVoiceCommand(VoiceCommandId id) {
  Serial.printf("[CommandManager] (voice/MultiNet) command=%s\n", voiceCommandIdToString(id));
  switch (id) {
    case VoiceCommandId::TAKE_PHOTO:      doTakePhoto(); break;
    case VoiceCommandId::START_VIDEO:     if (!_recording) doStartRecording(); break;
    case VoiceCommandId::STOP_VIDEO:      if (_recording) doStopRecording(); break;
    case VoiceCommandId::ANALYZE:
    case VoiceCommandId::WHAT_DO_YOU_SEE: doVisionQuestion(); break;
    case VoiceCommandId::BATTERY_STATUS:  doBatteryStatus(); break;
    case VoiceCommandId::RECORD_AUDIO:    doRecordAudio(); break;
    case VoiceCommandId::STOP_RECORDING:
      // Honest gap, not a fake stop: doRecordAudio() is a fixed-duration
      // (RECORD_AUDIO_MS) blocking capture with no mid-capture cancel
      // primitive today (see doRecordAudio()'s own comment). Saying
      // "stopped" here would claim a capability that doesn't exist yet.
      say("Audio recording runs for a fixed duration and can't be stopped early yet.");
      break;
    case VoiceCommandId::PLAY_MUSIC:      doPlayMusic(); break;
    case VoiceCommandId::STOP_MUSIC:
      // Same honest gap as above: v1's playback is a blocking stream, not
      // a background task, so there is no real mid-track "stop" distinct
      // from doPauseMusic()'s "don't auto-advance" behavior — see
      // doPauseMusic()'s own comment. Reuses it rather than inventing a
      // separate STOP_MUSIC behavior that doesn't actually exist.
      doPauseMusic();
      break;
    case VoiceCommandId::NONE:
    default:
      break;
  }
}

void CommandManager::doTakePhoto() {
  camera_fb_t* fb = _camera->capture();
  if (!fb) { say("Photo failed — camera not ready."); return; }
  String path = _sd->nextIndexedFilename("/OpenVisionEye/photos", "IMG", "jpg");
  bool ok = _sd->writeFile(path.c_str(), fb->buf, fb->len);
  esp_camera_fb_return(fb);
  if (ok) {
    Serial.println("[CommandManager] saved " + path);
    _wifi->sendControl(Proto::CMD_PHOTO_TAKEN);
    say("Photo taken.");
  } else {
    say("Photo failed — could not save to SD card.");
  }
}

void CommandManager::doStartRecording() {
  if (_recording) { say("Already recording."); return; }
  _currentVideoPath = _sd->nextIndexedFilename("/OpenVisionEye/videos", "VID", "ovemjpg");
  // Custom minimal container: repeated [uint32 frame_len][jpeg bytes]. See
  // docs/architecture.md — there is no on-device standard video encoder.
  if (!_sd->writeFile(_currentVideoPath.c_str(), nullptr, 0)) {
    say("Recording failed — could not create file.");
    return;
  }
  _recording = true;
  _videoStartedAt = millis();
  _wifi->sendControl(Proto::CMD_VIDEO_STARTED);
  say("Recording started.");
}

void CommandManager::doStopRecording() {
  if (!_recording) { say("Not recording."); return; }
  _recording = false;
  _wifi->sendControl(Proto::CMD_VIDEO_STOPPED);
  say("Recording stopped.");
  Serial.println("[CommandManager] saved " + _currentVideoPath);
}

void CommandManager::doRecordAudio() {
  // NOTE (v1.1.1 known limitation, same category as playCurrentTrack()
  // below): this blocks the main loop() for RECORD_AUDIO_MS (5s) — unlike
  // video recording, which is chunked one frame per loop() call, audio
  // capture here reuses AudioManager::recordToFile()'s existing blocking
  // loop rather than adding a second, separate non-blocking mic-capture
  // state machine on top of the one CommandManager::loop() already runs
  // for video. Camera/Wi-Fi/button polling all pause for those 5 seconds.
  // A future version could split this into per-loop chunks the same way
  // video recording already works, if that turns out to matter.
  if (!_sd || !_sd->isReady()) { say("Recording failed — SD card not ready."); return; }
  String path = _sd->nextIndexedFilename("/OpenVisionEye/audio", "REC", "wav");
  say("Recording audio.");
  bool ok = _audio->recordToFile(path, RECORD_AUDIO_MS);
  if (ok) {
    Serial.println("[CommandManager] saved " + path);
    say("Audio saved.");
  } else {
    say("Recording failed — see docs/hardware.md for the microphone setup.");
  }
}

void CommandManager::doSetVolume(int v) {
  v = constrain(v, 0, 100);
  if (_config) _config->setVolume(v);
  if (_wifi) _wifi->sendControl(String(Proto::PREFIX_VOLUME) + String(v));
  say("Volume set to " + String(v) + ".");
}

void CommandManager::refreshPlaylist() {
  _playlist = _sd->listFiles("/OpenVisionEye/music");
}

void CommandManager::playCurrentTrack() {
  // NOTE (v1 known limitation): this blocks the main loop() for the
  // duration of the track, so commands/diagnostics pause during playback.
  // Acceptable for a single-threaded v1 — see docs/architecture.md. A
  // future version could move this to a FreeRTOS task on the S3's second
  // core if that turns out to matter in practice.
  if (_playlistIndex < 0 || _playlistIndex >= (int)_playlist.size()) {
    say("No music found on the SD card.");
    return;
  }
  String path = _playlist[_playlistIndex];
  Serial.println("[CommandManager] streaming " + path);
  _wifi->sendControl(Proto::CMD_AUDIO_START);
  bool ok = _audio->streamFileTo(path, [this](const uint8_t* data, size_t len) {
    _audioBackend->sendPcm(data, len);
  });
  _wifi->sendControl(Proto::CMD_AUDIO_END);
  if (!ok) {
    say("Could not play that track — see docs/hardware.md for the expected WAV format.");
  }
}

void CommandManager::doPlayMusic() {
  if (_playlist.empty()) refreshPlaylist();
  if (_playlist.empty()) { say("No music found on the SD card."); return; }
  if (_playlistIndex < 0) _playlistIndex = 0;
  _wifi->sendControl(Proto::CMD_PLAY);
  say("Playing music.");
  playCurrentTrack();
}

void CommandManager::doPauseMusic() {
  // v1 has no background playback task to pause mid-stream (see
  // playCurrentTrack() note) — PAUSE here means "don't auto-advance",
  // and stops sending further chunks for the current call. A real pause/
  // resume needs the FreeRTOS-task version mentioned above.
  _wifi->sendControl(Proto::CMD_PAUSE);
  say("Music paused.");
}

void CommandManager::doNextSong() {
  if (_playlist.empty()) refreshPlaylist();
  if (_playlist.empty()) { say("No music found on the SD card."); return; }
  _playlistIndex = (_playlistIndex + 1) % (int)_playlist.size();
  _wifi->sendControl(Proto::CMD_NEXT);
  say("Next track.");
  playCurrentTrack();
}

void CommandManager::doPreviousSong() {
  if (_playlist.empty()) refreshPlaylist();
  if (_playlist.empty()) { say("No music found on the SD card."); return; }
  _playlistIndex = (_playlistIndex - 1 + (int)_playlist.size()) % (int)_playlist.size();
  _wifi->sendControl(Proto::CMD_PREVIOUS);
  say("Previous track.");
  playCurrentTrack();
}

void CommandManager::doBatteryStatus() {
  BatteryReading r = _battery->read();
  if (!r.hasReading) {
    say("Battery status is not available. See docs/hardware.md.");
    return;
  }
  if (BatteryManager::isLow(r)) {
    say("Battery is low, " + String(r.percent) + " percent.");
  } else {
    say("Battery is " + String(r.percent) + " percent.");
  }
}

void CommandManager::doAudioTest() {
  if (!_wifi->audioClientConnected()) {
    Serial.println("[CommandManager] AUDIO_TEST: Audio hub not connected on the audio channel (port 3334) yet — nothing to test.");
    return;
  }
  Serial.printf("[CommandManager] AUDIO_TEST: generating 440Hz/1s tone -> Wi-Fi -> ESP32 Audio -> A2DP -> headphones (backend: %s)\n",
    _audioBackend->active()->name());
  _wifi->sendControl(Proto::CMD_AUDIO_START);
  _audio->generateSineTone(440.0f, 1000, [this](const uint8_t* data, size_t len) {
    _audioBackend->sendPcm(data, len);
  });
  _wifi->sendControl(Proto::CMD_AUDIO_END);
  Serial.println("[CommandManager] AUDIO_TEST: tone sent. If you heard a steady 440Hz tone in your headphones, "
                  "the full audio pipeline works end-to-end. If not, see docs/troubleshooting.md.");
}

void CommandManager::doPingTest() {
  if (!_wifi->controlClientConnected()) {
    Serial.println("[CommandManager] PING_TEST: no Audio hub connected on the control channel (port 3333) yet.");
    return;
  }
  _pingPending = true;
  _pingSentAt = millis();
  _wifi->sendControl(Proto::CMD_PING);
  Serial.println("[CommandManager] PING_TEST: PING sent, waiting for PONG...");
}

void CommandManager::say(const String& text) {
  Serial.println("[SAY] " + text);
  if (_wifi) _wifi->sendControl(String(Proto::PREFIX_SAY) + text);
}

void CommandManager::handleControlLine(const String& line) {
  if (line == Proto::CMD_PING) {
    _wifi->sendControl(Proto::CMD_PONG);
  } else if (line == Proto::CMD_BATTERY) {
    BatteryReading r = _battery->read();
    String reply = String(Proto::PREFIX_BATTERY) + (r.hasReading ? String(r.percent) : String("UNKNOWN"));
    _wifi->sendControl(reply);
  } else if (line.startsWith(Proto::PREFIX_BATTERY)) {
    Serial.println("[CommandManager] Audio hub battery: " + line.substring(strlen(Proto::PREFIX_BATTERY)));
  } else if (line == Proto::CMD_PONG) {
    if (_pingPending) {
      unsigned long rtt = millis() - _pingSentAt;
      _pingPending = false;
      Serial.printf("[CommandManager] PONG received — round-trip time: %lu ms\n", rtt);
    }
    // else: unsolicited PONG (Audio hub's own liveness check), nothing to do
  } else {
    // Not a recognized Audio-hub protocol token. The control port also
    // doubles as a diagnostic channel a PC can connect to on the XIAO's
    // SoftAP (see tools/send_command.py and docs/architecture.md) — so
    // fall through to the same text-command path Serial input uses. This
    // is what actually makes "JARVIS" / "glasses" / "take a photo" /
    // "AUDIO_TEST" sent over TCP work, not just logged. NOTE: the control
    // server only holds one client at a time (see docs/wifi_protocol.md)
    // — using this from a PC and having the real Audio hub connected at
    // the same time will contend for that single slot.
    handleIncomingText(line);
  }
}
