// MultiNetSTT.h
// STATUS: 4 EXPERIMENTAL — real API calls against Espressif's own Arduino
// wrapper library, NOT hardware-verified, and NOT compiled by default. See
// docs/architecture.md "Part 1 — offline voice command recognition" for the
// full investigation and exactly why this is EXPERIMENTAL and not REAL.
//
// ---- What's actually verified vs. not, in one place ----
//
// VERIFIED (from Espressif's own repos, checked Aug 2026):
//  - `ESP_SR.h` / `ESP_SR` IS a real Arduino library, shipped INSIDE the
//    `arduino-esp32` core itself (arduino-esp32/libraries/ESP_SR) — this is
//    NOT the "must integrate ESP-IDF as a component" situation described
//    for wake-word in WakeWordEngine.h. You use it from a normal .ino with
//    `#include "ESP_SR.h"`, same as any other core library. Confirmed via
//    Espressif's own source (esp32-hal-sr.c) and the official Examples ->
//    ESP-SR -> Basic sketch that ships with the core.
//  - It IS ESP32-S3-only (`CONFIG_IDF_TARGET_ESP32S3`) — matches the XIAO.
//  - MultiNet's audio format is 16 kHz / 16-bit / mono — matches this
//    project's existing AudioManager::SAMPLE_RATE exactly, no reprocessing
//    needed on that front.
//  - English MultiNet models exist (mn5q8_en / mn6_en / mn7_en per
//    Espressif's esp-sr repo) and can recognize up to ~200-300 command
//    phrases — comfortably enough for your 10 initial commands.
//
// NOT VERIFIED / genuinely blocked (this is the honest part):
//  - The library's own source has a `#warning Compatible partition must be
//    selected for ESP_SR to work` guard tied to `ARDUINO_PARTITION_esp_sr_8`
//    / `_16` / `_32` build flags — i.e. it needs a special Tools > Partition
//    Scheme entry that reserves a MODEL partition. Vendor docs (e.g.
//    Waveshare's ESP32-S3 boards) show this as "ESP SR 16M (3MB APP / 7MB
//    SPIFFS / 2.9MB MODEL)" — sized for 16 MB flash boards. The XIAO ESP32S3
//    Sense has 8 MB flash. Whether the `esp_sr_8` variant (which the source
//    warning implies exists) is exposed as a Tools-menu option for the
//    specific "XIAO_ESP32S3" board entry, and whether the ~8 MB budget
//    (shared with this project's own sketch + OTA slot + SD-unrelated data)
//    leaves enough room for a MultiNet+WakeNet model set, is NOT confirmed.
//  - The library's `#if ... (CONFIG_MODEL_IN_FLASH || CONFIG_MODEL_IN_SDCARD)`
//    guard shows a "load the model from the SD card instead of a flash
//    partition" path DOES exist at the underlying esp-sr component level —
//    which would sidestep the 8 MB flash-budget problem entirely, since
//    this project already has an SD card. But there is no confirmed,
//    documented way to select `CONFIG_MODEL_IN_SDCARD` from the Arduino IDE
//    Tools menu (Arduino IDE exposes only whatever `boards.txt` defines as
//    menu options — this was not found in the board definition for
//    "XIAO_ESP32S3"/"XIAO_ESP32S3(_PLUS)"). Not hardware verified, not
//    implemented here — flagged as the single biggest open question for
//    whoever picks this back up.
//  - A dated, reproducible failure report (Arduino Forum, March 2026) shows
//    someone running the exact official "ESP-SR -> Basic" example, with
//    board = "XIAO_ESP32S3(_PLUS)" selected (i.e. this exact board family),
//    crashing at boot with `E MODEL_LOADER: Can not find model in partition
//    table` followed by a memory-exhausted panic — before any of your own
//    code runs. Several other, similar reports ("Errno2 looking for
//    srmodels.bin", "ESP-SR compile error", "ESP_SR works ... sometimes")
//    span roughly Jan 2025 - Apr 2026, suggesting this is a recurring
//    friction point on Arduino IDE generally, not a one-off fluke.
//
// ---- Verdict (see docs/architecture.md for the full writeup) ----
// Arduino IDE + MultiNet is a REAL, non-fake code path (this file uses the
// actual `ESP_SR` API — `sr_cmd_t`, `ESP_SR.begin()`, `SR_EVENT_COMMAND`,
// etc., not invented ones) but is NOT proven to boot on this exact 8 MB
// XIAO ESP32S3 Sense board without hardware in hand to work through the
// partition-table blocker above. It ships here OFF BY DEFAULT
// (`OVE_ENABLE_MULTINET` undefined) so it never silently breaks a normal
// build. VOICE_MODE stays "OFF" by default in config.json (see
// ConfigManager.h) — set it to "MULTINET" only if you've confirmed on your
// own hardware that it boots, per docs/installation.md "Enabling MultiNet
// (experimental)".
//
// ---- Command ID mapping (per your spec) ----
// This class does NOT do "speech-to-text" — MultiNet recognizes a fixed
// command *identity* directly (a `command_id` from the `sr_cmd_t` table you
// give it), which IS the "Command ID" your spec asked for. The `command_id`
// values here are literally `VoiceCommandId` (see VoiceCommandIds.h) cast to
// int, so ESP-SR's own recognition result requires no further translation
// before reaching CommandManager.

#pragma once
#include <Arduino.h>
#include "STTEngine.h"
#include "VoiceCommandIds.h"

#ifdef OVE_ENABLE_MULTINET
#include "ESP_I2S.h"
#include "ESP_SR.h"
#endif

// Non-blocking, event-driven (not a poll(String&)-shaped STTEngine — voice
// *commands*, not general speech-to-text, so it exposes command IDs
// directly instead of forcing a round-trip through text). Kept separate
// from the STTEngine interface on purpose; see STTEngine.h for why a
// generic STT interface and a fixed-vocabulary command engine are
// different problems.
class MultiNetSTT {
public:
  // Returns true and fills `outId` exactly once per recognized command
  // (call every loop() iteration; returns false almost every call).
  // Always returns false, immediately, if OVE_ENABLE_MULTINET was not
  // defined at build time or begin() was never called/failed — this is
  // never wired into the main firmware unless VOICE_MODE=MULTINET in
  // config.json (see ConfigManager.h / CommandManager.cpp).
  bool begin();
  bool poll(VoiceCommandId& outId);
  bool isReady() const { return _ready; }

private:
  bool _ready = false;

#ifdef OVE_ENABLE_MULTINET
  I2SClass _i2s;
  static void onEventStatic(sr_event_t event, int command_id, int phrase_id);
  static volatile int s_lastCommandId; // -1 = none pending
  static volatile bool s_pending;
#endif
};
