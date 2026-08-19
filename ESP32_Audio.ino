// ESP32_Audio.ino
// STATUS: 1 REAL (orchestration; see each module's own STATUS comment)
//
// Board: any CLASSIC ESP32 (Xtensa, dual-core) — e.g. "ESP32 Dev Module" in
// Arduino IDE. NOT S2/S3/C3/C6 — see docs/hardware.md §2.1 for why this is
// a hard requirement (Bluetooth Classic A2DP does not exist on those chips).
//
// Role: Wi-Fi station -> connects to the XIAO's SoftAP -> forwards PCM
// audio to a Bluetooth Classic A2DP source (your headphones/earbuds).
// See docs/wifi_protocol.md for the two-port protocol and
// docs/architecture.md for the overall system design.

#include "Config.h"
#include "WiFiAudio.h"
#include "BluetoothAudio.h"
#include "AudioOutput.h"
#include "BatteryManager.h"
#include <string.h>

WiFiAudio wifiAudio;
BluetoothAudio bluetoothAudio;
AudioOutput audioOutput;
BatteryManager batteryManager;

void handleControlLine(const String& line);

void setup() {
  Serial.begin(115200);
  unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 3000) { delay(10); }

  Serial.println();
  Serial.println("OpenVisionEye Audio Hub starting...");

  batteryManager.begin(false); // no circuit configured by default — see docs/hardware.md §2.5

  bool audioOk = audioOutput.begin();
  bool btOk = bluetoothAudio.begin(AudioHubConfig::HEADSET_BT_NAME, &audioOutput);

  wifiAudio.setControlLineHandler(handleControlLine);
  wifiAudio.setPcmChunkHandler([](const uint8_t* data, size_t len) {
    audioOutput.pushMonoPcm(data, len);
  });

  bool wifiOk = wifiAudio.begin(AudioHubConfig::AP_SSID_PREFIX, AudioHubConfig::AP_PASSWORD,
                                 AudioHubConfig::XIAO_AP_IP);

  Serial.println("---- DIAGNOSTIC MODE ----");
  Serial.printf("[%s] Wi-Fi link to XIAO\n", wifiOk ? "OK" : "FAIL");
  Serial.printf("[%s] Audio output buffer\n", audioOk ? "OK" : "FAIL");
  Serial.printf("[%s] Bluetooth A2DP source started\n", btOk ? "OK" : "FAIL");
  Serial.println("[--] Bluetooth headset link — connects once paired, see installation.md");
  Serial.println("-------------------------");

  if (wifiOk) {
    wifiAudio.sendControl("Audio hub connected.");
    // Per docs/wifi_protocol.md this is really meant to be sent as SAY:...
    // by the XIAO; we also announce our own presence so Serial logs on
    // both boards show the "first test" flow from the spec immediately.
  }

  Serial.println("Ready.");
}

void loop() {
  wifiAudio.loop();

  static unsigned long lastBatteryPing = 0;
  if (millis() - lastBatteryPing > 30000) {
    lastBatteryPing = millis();
    BatteryReading r = batteryManager.read();
    String reply = String(Proto::PREFIX_BATTERY) + (r.hasReading ? String(r.percent) : String("UNKNOWN"));
    wifiAudio.sendControl(reply);
  }
}

void handleControlLine(const String& line) {
  Serial.println("[ControlLine] " + line);

  if (line == Proto::CMD_PING) {
    wifiAudio.sendControl(Proto::CMD_PONG);
  } else if (line == Proto::CMD_PLAY) {
    Serial.println("[AudioHub] PLAY (A2DP will play buffered audio as it arrives)");
  } else if (line == Proto::CMD_PAUSE) {
    Serial.println("[AudioHub] PAUSE (see CommandManager.cpp note on the XIAO side — v1 has no true pause)");
  } else if (line == Proto::CMD_STOP) {
    Serial.println("[AudioHub] STOP");
  } else if (line == Proto::CMD_NEXT) {
    Serial.println("[AudioHub] NEXT");
  } else if (line == Proto::CMD_PREVIOUS) {
    Serial.println("[AudioHub] PREVIOUS");
  } else if (line == Proto::CMD_BATTERY) {
    BatteryReading r = batteryManager.read();
    String reply = String(Proto::PREFIX_BATTERY) + (r.hasReading ? String(r.percent) : String("UNKNOWN"));
    wifiAudio.sendControl(reply);
  } else if (line.startsWith(Proto::PREFIX_BATTERY)) {
    Serial.println("[AudioHub] XIAO battery: " + line.substring(strlen(Proto::PREFIX_BATTERY)));
  } else if (line.startsWith(Proto::PREFIX_VOLUME)) {
    int v = line.substring(strlen(Proto::PREFIX_VOLUME)).toInt();
    v = constrain(v, 0, 100);
    uint8_t btVolume = (uint8_t)map(v, 0, 100, 0, 127);
    bluetoothAudio.setVolume(btVolume);
  } else if (line.startsWith(Proto::PREFIX_SAY)) {
    String text = line.substring(strlen(Proto::PREFIX_SAY));
    // v1 has no local TTS on this board either — see docs/architecture.md.
    // We log it so you can see the pipeline is alive end-to-end; wiring
    // this to actual canned prompt playback is a small, well-contained
    // follow-up (play a matching WAV from a small phrase table over the
    // same AudioOutput path used for music).
    Serial.println("[SAY] " + text);
  } else if (line == Proto::CMD_PHOTO_TAKEN || line == Proto::CMD_VIDEO_STARTED ||
             line == Proto::CMD_VIDEO_STOPPED || line == Proto::CMD_AUDIO_START ||
             line == Proto::CMD_AUDIO_END || line == Proto::CMD_PONG) {
    // Informational events from the XIAO — nothing to do here beyond the
    // log line above, in v1.
  } else {
    Serial.println("[AudioHub] unrecognized control line: " + line);
  }
}
