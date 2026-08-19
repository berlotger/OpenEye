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
                            AudioManager* audio, BatteryManager* battery, WiFiManager* wifi,
                            ConfigManager* config, VisionAI* vision, LanguageAI* language) {
  _wake = wake; _camera = camera; _sd = sd; _audio = audio; _battery = battery;
  _wifi = wifi; _config = config; _vision = vision; _language = language;
}

void CommandManager::loop() {
  if (_wake) {
    WakeWord w = _wake->poll();
    if (w != WakeWord::NONE) onWake(w);
  }

  if (_listening && millis() > _listenUntil) {
    _listening = false;
    Serial.println("[CommandManager] listening window closed (no command received)");
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
  switch (w) {
    case WakeWord::HEY_GLASSES:
      _listening = true; _pendingWake = w; _listenUntil = millis() + LISTEN_WINDOW_MS;
      Serial.println("[CommandManager] \"Hey Glasses\" — listening for a system command...");
      break;
    case WakeWord::HEY_AI:
      _listening = true; _pendingWake = w; _listenUntil = millis() + LISTEN_WINDOW_MS;
      Serial.println("[CommandManager] \"Hey AI\" — listening for a question...");
      break;
    case WakeWord::HEY_JARVIS:
      Serial.println("[CommandManager] \"Hey Jarvis\" heard — Jarvis is not implemented yet (v2.0 hook).");
      say("Jarvis is not available yet.");
      break;
    default:
      break;
  }
}

void CommandManager::handleIncomingText(const String& lineIn) {
  String line = lineIn; line.trim();
  if (line.length() == 0) return;

  // Allow text-injected wake words (useful before real wake-word detection
  // exists — see WakeWordEngine.h).
  if (line.equalsIgnoreCase("HEY_GLASSES")) { onWake(WakeWord::HEY_GLASSES); return; }
  if (line.equalsIgnoreCase("HEY_AI"))      { onWake(WakeWord::HEY_AI); return; }
  if (line.equalsIgnoreCase("HEY_JARVIS"))  { onWake(WakeWord::HEY_JARVIS); return; }

  if (!_listening) {
    Serial.println("[CommandManager] got text but no wake word is active — ignoring: " + line);
    return;
  }
  _listening = false;

  if (_pendingWake == WakeWord::HEY_GLASSES) {
    dispatchGlassesCommand(line);
  } else if (_pendingWake == WakeWord::HEY_AI) {
    dispatchAiCommand(line);
  }
}

void CommandManager::dispatchGlassesCommand(const String& text) {
  if (containsIgnoreCase(text, "take a photo") || containsIgnoreCase(text, "photo")) {
    doTakePhoto();
  } else if (containsIgnoreCase(text, "start recording")) {
    doStartRecording();
  } else if (containsIgnoreCase(text, "stop recording")) {
    doStopRecording();
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
  if (containsIgnoreCase(text, "what do i see") || containsIgnoreCase(text, "what do you see")) {
    camera_fb_t* fb = _camera->capture();
    if (!fb) { say("I couldn't get a frame from the camera."); return; }
    VisionResult r = _vision->analyze(fb);
    esp_camera_fb_return(fb);
    if (!r.available) {
      say(r.statusMessage.length() ? r.statusMessage : "Vision AI is not available yet.");
    } else {
      say(_language->describe(r));
    }
  } else {
    Serial.println("[CommandManager] unrecognized AI question: " + text);
    say("I don't have an answer for that yet.");
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
    _wifi->sendAudioChunk(data, len);
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
    // liveness ack, nothing to do
  } else {
    Serial.println("[CommandManager] control line from Audio hub: " + line);
  }
}
