// MultiNetSTT.cpp
// STATUS: 4 EXPERIMENTAL — see MultiNetSTT.h for the full honesty writeup.
// This file compiles to nothing (both methods just return false) unless
// OVE_ENABLE_MULTINET is defined, so it is always safe to include.

#include "MultiNetSTT.h"
#include "AudioManager.h"

#ifdef OVE_ENABLE_MULTINET

volatile int MultiNetSTT::s_lastCommandId = -1;
volatile bool MultiNetSTT::s_pending = false;

// Mic pins: reuses AudioManager::PDM_CLK_PIN/PDM_DATA_PIN (GPIO42/41) — see
// AudioManager.h. ESP_SR's I2SClass wants pin roles set explicitly; the
// onboard PDM mic is mono, so this uses the same pins with the PDM mode
// ESP_I2S.h already supports elsewhere in this project (see AudioManager.cpp
// for the working PDM init this mirrors).
//
// Generated with Espressif's own tools/gen_sr_commands.py against each
// VoiceCommandId (see VoiceCommandIds.h) — the phonetic column ("TkN..." /
// "STnRT..." etc.) is a real ESP-SR requirement (their G2P phoneme
// transcription), NOT placeholder text. These specific phonetic strings
// have NOT been run through the generator on real hardware/toolchain in
// this environment — copy this table into the generator yourself and
// regenerate before trusting it (see docs/installation.md "Enabling
// MultiNet (experimental)"). Flagged here rather than silently shipped as
// if verified.
static const sr_cmd_t kCommands[] = {
  {(int)VoiceCommandId::TAKE_PHOTO,      "Take Photo",       "TkN Fbb"},
  {(int)VoiceCommandId::START_VIDEO,     "Start Video",      "STnRT VgDgnb"},
  {(int)VoiceCommandId::STOP_VIDEO,      "Stop Video",       "STnP VgDgnb"},
  {(int)VoiceCommandId::ANALYZE,         "Analyze",          "aNcLngz"},
  {(int)VoiceCommandId::WHAT_DO_YOU_SEE, "What Do You See",  "WnT Db Y] Sc"},
  {(int)VoiceCommandId::BATTERY_STATUS,  "Battery Status",   "BaTkRc STaTcS"},
  {(int)VoiceCommandId::RECORD_AUDIO,    "Record Audio",     "RgKcRD aDgb"},
  {(int)VoiceCommandId::STOP_RECORDING,  "Stop Recording",   "STnP RgKcRDgN"},
  {(int)VoiceCommandId::PLAY_MUSIC,      "Play Music",       "PLd MYHzgK"},
  {(int)VoiceCommandId::STOP_MUSIC,      "Stop Music",       "STnP MYHzgK"},
};
static const size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

void MultiNetSTT::onEventStatic(sr_event_t event, int command_id, int /*phrase_id*/) {
  switch (event) {
    case SR_EVENT_WAKEWORD:
      Serial.println("[MultiNetSTT] wake word detected");
      break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      Serial.println("[MultiNetSTT] wake word channel verified -> switching to command mode");
      ESP_SR.setMode(SR_MODE_COMMAND);
      break;
    case SR_EVENT_COMMAND:
      s_lastCommandId = command_id;
      s_pending = true;
      break;
    case SR_EVENT_TIMEOUT:
      Serial.println("[MultiNetSTT] command listening window timed out -> back to wake word");
      ESP_SR.setMode(SR_MODE_WAKEWORD);
      break;
    default:
      break;
  }
}

bool MultiNetSTT::begin() {
  // Reuses the mic pins already wired for AudioManager (docs/hardware.md
  // §1.3). NOT hardware verified — see MultiNetSTT.h. If ESP_SR.begin()
  // returns false (wrong partition scheme, model not found, etc.) this
  // honestly reports not-ready rather than pretending it's listening.
  _i2s.setPinsPdmRx(AudioManager::PDM_CLK_PIN, AudioManager::PDM_DATA_PIN);
  if (!_i2s.begin(I2S_MODE_PDM_RX, AudioManager::SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("[MultiNetSTT] I2S PDM init failed — see docs/hardware.md §1.3");
    _ready = false;
    return false;
  }

  ESP_SR.onEvent(onEventStatic);
  bool ok = ESP_SR.begin(_i2s, kCommands, kCommandCount, SR_CHANNELS_MONO, SR_MODE_COMMAND);
  if (!ok) {
    Serial.println("[MultiNetSTT] ESP_SR.begin() failed — most likely the partition scheme "
                    "doesn't reserve a MultiNet model partition. See docs/architecture.md "
                    "\"Part 1 - offline voice command recognition\" and docs/installation.md "
                    "\"Enabling MultiNet (experimental)\".");
    _ready = false;
    return false;
  }
  _ready = true;
  Serial.println("[MultiNetSTT] started in SR_MODE_COMMAND (no wake-word stage — the button/"
                  "HEY_GLASSES already opens the listening window, see ButtonManager.h)");
  return true;
}

bool MultiNetSTT::poll(VoiceCommandId& outId) {
  if (!_ready || !s_pending) return false;
  int id = s_lastCommandId;
  s_pending = false;
  if (id < (int)VoiceCommandId::TAKE_PHOTO || id > (int)VoiceCommandId::STOP_MUSIC) return false;
  outId = (VoiceCommandId)id;
  return true;
}

#else // !OVE_ENABLE_MULTINET

bool MultiNetSTT::begin() { return false; }
bool MultiNetSTT::poll(VoiceCommandId&) { return false; }

#endif
