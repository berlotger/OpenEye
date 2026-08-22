# OpenVisionEye v1.2 AI Offline — Hardware / Feature Status

Single source of truth for what actually works today vs. what's a stub,
interface, or future item. Every row reflects the real code as reviewed,
not aspirational goals. Status legend matches `docs/architecture.md`:

- **REAL** — works today, no extra hardware/library beyond the base BOM.
- **NEEDS-LIBRARY** — works today, requires a third-party Arduino library
  (see `requirements.md`).
- **NEEDS-HARDWARE** — interface + implementation exist; needs hardware
  you haven't added yet (e.g. a battery ADC divider).
- **EXPERIMENTAL** — a real implementation exists but is a stub you should
  expect to replace, not audio/keyword detection on real signals.
- **NOT-IMPLEMENTED** — interface exists, no working implementation yet.
- **FUTURE** — interface only, not realistically achievable on this
  hardware today.

| Feature | Status | Requirement | Notes |
|---|---|---|---|
| Camera capture (photo) | REAL | XIAO + OV2640/OV3660, PSRAM | `CameraManager`, verified pin map, JPEG to SD, zero-padded filenames (`IMG_000001.jpg`) |
| microSD storage | REAL | microSD card, SPI on GPIO21 | `SDManager`, folder tree auto-created, GPIO21 flash-LED conflict avoided on purpose |
| Onboard PDM microphone | REAL | XIAO built-in mic | `AudioManager`, `ESP_I2S.h`, blocking capture loop (acceptable for v1, documented) |
| Mic → WAV recording to SD | REAL | microSD | `AudioManager::recordToFile()`, wired to the "record audio" command in v1.1.1 (`CommandManager::doRecordAudio()`), fixed 5s duration, saves to `/OpenVisionEye/audio/REC_NNNNNN.wav`. Blocks the main loop for those 5s — same documented tradeoff as music playback |
| Wi-Fi link (SoftAP + station) | REAL | both boards | `WiFiManager` (XIAO) / `WiFiAudio` (Audio hub), two TCP ports, single client each |
| Music playback (WAV → BT) | REAL | WAV file must be mono/16-bit/16kHz PCM | No MP3/AAC decoder — convert with `ffmpeg` first, see `requirements.md` |
| Bluetooth Classic A2DP output | NEEDS-LIBRARY | classic ESP32 + ESP32-A2DP lib | `BluetoothAudio`; will not compile on S2/S3/C3/C6 by design |
| 16kHz mono → 44.1kHz stereo resampling | REAL | — | `AudioOutput`, zero-order-hold upsampling (voice-grade, not audiophile), ring buffer with critical-section locking |
| `AUDIO_TEST` diagnostic tone | REAL | both boards + headphones paired | New in v1.1 — 440Hz/1s tone through the real Wi-Fi→A2DP pipeline, not a local-only loopback |
| `PING_TEST` round-trip latency | REAL | both boards connected | New in v1.1 — measured, not estimated |
| Config file (`config.json`) | REAL | needs ArduinoJson | `ConfigManager`, no internet credentials ever stored |
| Battery status (XIAO) | NEEDS-HARDWARE | DIY ADC voltage divider | Reports "not available", never a fabricated percentage, until the divider is wired |
| Battery status (Audio hub) | NEEDS-HARDWARE | same as above | Same honesty policy |
| Button long-press → command mode (PRIMARY trigger) | REAL | a push-button wired to GPIO2/"D1" + GND (`BUTTON_GPIO = 2`) | v1.1.1 — `ButtonManager::COMMAND_MODE` opens ONE command-listening window directly (spec §7/§8: the button is the wake-word substitute this version). Non-blocking single/double/long click, debounced, no `delay()`. **Optional hardware**: `HEY_GLASSES` typed on Serial/TCP opens the identical window without the physical button wired |
| Button mapping, runtime-configurable | REAL | — | `BUTTON_SINGLE:`/`BUTTON_DOUBLE:`/`BUTTON_LONG:` text commands, persisted to `config.json`, no reflash needed |
| Wake word ("Jarvis") — SECONDARY/legacy, disabled by default | EXPERIMENTAL | — | `ManualTriggerWakeWord`: BOOT-button press (gated off by default — `config.json`'s `wakeWordEnabled: false`, see `ConfigManager.h`) or literal `JARVIS`/`HEY_JARVIS` text over Serial/TCP (always available, any `wakeWordEnabled` value). **Not** audio keyword spotting. See `docs/architecture.md` — as of Aug 2026, Espressif's `esp-sr` component now ships an official pretrained "Jarvis" WakeNet9 model (`wn9_jarvis_tts`, ESP32-S3), which **corrects** this project's earlier claim that no native "Jarvis" model exists. It is still not implemented here — porting an ESP-IDF component into this bare-Arduino sketch remains real, non-trivial work, and you explicitly asked not to build it this version |
| Mode selection ("glasses" / "ai" / "music") — part of the legacy Jarvis flow | mixed | — | `CommandManager::onModeWord()` — "glasses"/"ai" are REAL (open a real command window); "music" is recognized text but replies "not available yet" (NOT-IMPLEMENTED) |
| `HEY_AI` bench shortcut | REAL | — | Skips straight to the AI-question window without needing Jarvis + a mode word first |
| AudioBackend: LOCAL | REAL | — | New in v1.1 — same Wi-Fi → ESP32 Audio → A2DP path v1.0 always used, now reachable through an `AudioBackend` interface |
| AudioBackend: PHONE | NOT-IMPLEMENTED | a companion phone app + a real transport (none exists) | New in v1.1 — `PhoneAudioBackend` always reports itself unavailable and always fails to send. Exists only so `AUDIO_MODE` has somewhere real to point later — see `AudioBackend.h` |
| AudioBackend: AUTO | REAL (resolves to LOCAL today) | — | New in v1.1 — re-resolves every audio chunk; falls back to LOCAL automatically since PHONE is never available. Will start preferring PHONE automatically, with no code changes elsewhere, once that backend is real |
| STT interface (`STTEngine.h`) | FUTURE | interface only | New in v1.1 — `LocalSTT`/`OnlineSTT`/`PhoneSTT` would implement this later. **Deliberately not wired into `CommandManager`** — see the file's own header comment for why wiring a `ManualSTT` today would be cosmetic, not real STT |
| Speech-to-text (actual decoded speech) | NOT-IMPLEMENTED | a real STT engine | Command *text* still comes from Serial/TCP typed input, not from decoded speech |
| Text-to-speech (TTS) | NOT-IMPLEMENTED | a real TTS engine or WAV phrase bank | `SAY:<text>` is sent and logged on the Audio hub; it does **not** produce audio. See "TTS" in `docs/architecture.md` |
| Volume command ("volume \<n\>") | REAL | — | v1.1.1 — `CommandManager::doSetVolume()`: saves to `config.json` and sends `VOLUME:<n>` to the Audio hub, which maps it to `BluetoothA2DPSource::set_volume()`. Previously-dead protocol plumbing (the Audio hub always understood `VOLUME:`; the XIAO never sent it) — now wired end-to-end |
| Video recording | REAL (custom format) | microSD | Concatenated `[len][JPEG]` frames, capped at 15s/clip — **not** MP4/AVI, not playable in standard video players |
| Vision AI (object detection) | mixed | `NullVisionAI` (default, `VISION_MODE=OFF`) REAL; `EdgeImpulseVisionAI` (`VISION_MODE=LOCAL`) EXPERIMENTAL | New in v1.2: `EdgeImpulseVisionAI` is a real integration point against the confirmed-working (on this board, by others) Edge Impulse Arduino export API, gated behind `OVE_ENABLE_EDGE_IMPULSE` — but it needs YOUR OWN trained model (person/shoe/bottle), which this project cannot ship. Not hardware verified. `NullVisionAI` stays the default and always honestly reports "not available" |
| Position (LEFT/CENTER/RIGHT) from bounding box | REAL | — | New in v1.2 — `computePosition()` in `VisionAI.h`, plain arithmetic, no AI |
| Depth estimate (VERY_NEAR/NEAR/MEDIUM/FAR) | REAL | — | New in v1.2 — `computeDepth()` in `VisionAI.h`, bucketed bounding-box-area ratio, explicitly documented as an estimate, never real distance |
| MultiNet offline voice commands | EXPERIMENTAL | `OVE_ENABLE_MULTINET` + a partition scheme with a MultiNet model partition (NOT confirmed to exist for this board at 8MB flash) | New in v1.2: `MultiNetSTT` uses the real, built-in `ESP_SR` Arduino library — genuinely different situation from the Jarvis wake-word row above (no ESP-IDF component juggling needed) — but a dated (March 2026) forum report shows the stock example crashing at boot on this exact board with `MODEL_LOADER: Can not find model in partition table`. Off by default (`VOICE_MODE=OFF`). See `docs/architecture.md` "Part 1" |
| Language AI (scene description) | REAL (template only) / FUTURE (real model) | — | `TemplateLanguageAI` builds a sentence from whatever `VisionAI` returns — real templating either way, regardless of which `VisionAI` implementation is active |
| On-device LLM | FUTURE | far more RAM/flash than the XIAO has | Explicitly ruled out as a hardware ceiling (8MB PSRAM/flash total), not a missing library |
| Cloud/internet AI | NOT-IMPLEMENTED | Wi-Fi internet uplink, none exists in v1 | v1 is offline-only by design |
| PC-driven text commands over TCP (`tools/send_command.py`) | REAL | PC on the XIAO's SoftAP | Unrecognized control-channel lines fall through to the same command path Serial uses, including the v1.1 `JARVIS`/`BUTTON_*` commands. Single-client limitation applies (see `docs/wifi_protocol.md`) |

## Compilation

Not verified in this environment — no Arduino CLI/toolchain or physical
hardware available here. All changes were checked statically: every
`#include "*.h"` resolves to a real file in the same sketch folder (or a
core/library header), every method declared in a `.h` has a matching
`ClassName::method` definition in the corresponding `.cpp` (and vice
versa — no orphaned definitions), `CommandManager::begin()`'s call sites in
`XIAO_OpenVisionEye.ino` match its declared parameter list, and both
boards' `ProtocolDefs.h` copies still match byte-for-byte on the constants
(only comments differ, which was already true before this revision — see
`docs/wifi_protocol.md`). Build it yourself with Arduino IDE per
`requirements.md` and `docs/installation.md` before flashing.
