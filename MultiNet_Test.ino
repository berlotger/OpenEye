// MultiNet_Test.ino
// STATUS: 4 EXPERIMENTAL — see the big comment at the top of
// ../../XIAO_OpenVisionEye/MultiNetSTT.h before running this. This sketch
// is real code against Espressif's real ESP_SR Arduino API — it is NOT
// hardware-verified in this environment, and a dated (March 2026) forum
// report shows the stock official example crashing at boot on this exact
// board family with the wrong partition scheme selected. Expect to need to
// debug the partition scheme yourself; see docs/installation.md "Enabling
// MultiNet (experimental)".
//
// Purpose (per spec): verify voice recognition works in ISOLATION —
//   XIAO microphone -> ESP-SR MultiNet -> print the detected command to
//   Serial
// Deliberately excludes: camera, Wi-Fi, Bluetooth. If this doesn't work on
// its own, it will not magically work once integrated into the main
// firmware — debug it here first.
//
// ---- Required before this will even compile ----
// 1. Arduino IDE board: XIAO_ESP32S3 (or "XIAO_ESP32S3(_PLUS)" if that's
//    what your Boards Manager shows).
// 2. Tools > Partition Scheme: you need one that reserves a MultiNet MODEL
//    partition (look for a scheme with "SR" or "esp_sr" in its name; on
//    16MB-flash boards Espressif's own docs show
//    "ESP SR 16M (3MB APP/7MB SPIFFS/2.9MB MODEL)" as an example — whether
//    an 8MB-flash equivalent is exposed for XIAO_ESP32S3 specifically is
//    NOT confirmed; this is the actual open question, not a guess to paper
//    over). If no such scheme appears in the Partition Scheme dropdown for
//    this board, that IS the blocker — see docs/architecture.md.
// 3. Tools > PSRAM: OPI PSRAM enabled.
//
// If you get `E MODEL_LOADER: Can not find model in partition table` at
// boot, that's the exact known failure mode from the forum report above —
// it means step 2 wasn't satisfied, not that your code is wrong.

#include "ESP_I2S.h"
#include "ESP_SR.h"

// PDM mic pins verified elsewhere in this project for the XIAO ESP32-S3
// Sense's onboard mic (see ../../XIAO_OpenVisionEye/AudioManager.h §hardware
// notes) — CLK=GPIO42, DATA=GPIO41.
#define PDM_CLK_PIN 42
#define PDM_DATA_PIN 41

I2SClass i2s;

// The 10 initial commands from the spec. Command IDs match
// ../../XIAO_OpenVisionEye/VoiceCommandIds.h exactly, so a command
// recognized here is identical to what the main firmware would see.
//
// IMPORTANT: the third column (phonetic transcription) below was NOT
// regenerated with Espressif's own tools/gen_sr_commands.py against real
// hardware/toolchain in this environment — see MultiNetSTT.cpp for the
// same caveat. Regenerate it yourself before trusting recognition
// accuracy:
//   python3 tools/gen_sr_commands.py "Take Photo;Start Video;Stop Video;Analyze;What Do You See;Battery Status;Record Audio;Stop Recording;Play Music;Stop Music"
enum {
  CMD_TAKE_PHOTO = 0,
  CMD_START_VIDEO,
  CMD_STOP_VIDEO,
  CMD_ANALYZE,
  CMD_WHAT_DO_YOU_SEE,
  CMD_BATTERY_STATUS,
  CMD_RECORD_AUDIO,
  CMD_STOP_RECORDING,
  CMD_PLAY_MUSIC,
  CMD_STOP_MUSIC,
};

static const sr_cmd_t sr_commands[] = {
  {CMD_TAKE_PHOTO,      "Take Photo",       "TkN Fbb"},
  {CMD_START_VIDEO,     "Start Video",      "STnRT VgDgnb"},
  {CMD_STOP_VIDEO,      "Stop Video",       "STnP VgDgnb"},
  {CMD_ANALYZE,         "Analyze",          "aNcLngz"},
  {CMD_WHAT_DO_YOU_SEE, "What Do You See",  "WnT Db Y] Sc"},
  {CMD_BATTERY_STATUS,  "Battery Status",   "BaTkRc STaTcS"},
  {CMD_RECORD_AUDIO,    "Record Audio",     "RgKcRD aDgb"},
  {CMD_STOP_RECORDING,  "Stop Recording",   "STnP RgKcRDgN"},
  {CMD_PLAY_MUSIC,      "Play Music",       "PLd MYHzgK"},
  {CMD_STOP_MUSIC,      "Stop Music",       "STnP MYHzgK"},
};

void onSrEvent(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_COMMAND:
      Serial.printf("[MultiNet_Test] COMMAND DETECTED: id=%d phrase=\"%s\"\n",
                     command_id, sr_commands[phrase_id].str);
      break;
    case SR_EVENT_TIMEOUT:
      Serial.println("[MultiNet_Test] timeout, still listening (SR_MODE_COMMAND stays active)");
      break;
    default:
      Serial.printf("[MultiNet_Test] event=%d (unused in this test — no wake word here, see comment)\n", (int)event);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) { delay(10); }

  Serial.println();
  Serial.println("MultiNet_Test starting (mic only — no camera, no Wi-Fi, no Bluetooth)...");

  i2s.setPinsPdmRx(PDM_CLK_PIN, PDM_DATA_PIN);
  if (!i2s.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("[FAIL] I2S PDM mic init failed. Halting.");
    while (true) delay(1000);
  }
  Serial.println("[OK] Microphone initialized");

  ESP_SR.onEvent(onSrEvent);
  // SR_MODE_COMMAND directly, no wake-word stage — per spec this version
  // has no wake word; the button (or, here, just always-listening) is the
  // trigger. See ../../XIAO_OpenVisionEye/WakeWordEngine.h for why wake
  // word stays FUTURE project-wide.
  bool ok = ESP_SR.begin(i2s, sr_commands, sizeof(sr_commands) / sizeof(sr_commands[0]),
                          SR_CHANNELS_MONO, SR_MODE_COMMAND);
  Serial.printf("[%s] MultiNet model loaded\n", ok ? "OK" : "FAIL");
  if (!ok) {
    Serial.println("See the big comment at the top of this file — this is almost certainly");
    Serial.println("the partition-scheme blocker, not a code bug. Halting.");
    while (true) delay(1000);
  }

  Serial.println("Ready. Say one of the 10 commands (see the table in this file).");
}

void loop() {
  // ESP_SR runs its own internal task; nothing to poll here. Kept empty on
  // purpose, matching the official Basic example's structure.
}
