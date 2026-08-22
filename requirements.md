# OpenVisionEye — Requirements

## Arduino IDE

Arduino IDE **2.x** (tested workflow assumes 2.x; 1.8.19 should also work but
board manager URLs are entered differently — see `docs/installation.md`).

## Board packages (Boards Manager)

| Package | Used for | Notes |
|---|---|---|
| `esp32` by Espressif Systems | both boards | Install via Boards Manager after adding the Espressif package index URL — see `docs/installation.md`. Use a **current 3.x release** (not an ancient 1.0.x install) so `ESP_I2S.h` and current camera driver behavior are available. |

## Board selection

| Sketch | Board to select |
|---|---|
| `XIAO_OpenVisionEye/XIAO_OpenVisionEye.ino` | **XIAO_ESP32S3** (search "xiao s3" in the board dropdown) |
| `ESP32_Audio/ESP32_Audio.ino` | **ESP32 Dev Module** (or your specific classic-ESP32 board entry) — must be a classic ESP32, not S2/S3/C3/C6, see `docs/hardware.md` §2.1 |

## Arduino Library Manager dependencies

| Library | Author | Used by | Why |
|---|---|---|---|
| `ArduinoJson` | Benoit Blanchon | XIAO (`ConfigManager`) | JSON read/write for `config.json` on SD. Install the current 7.x release — the code uses the v7 `JsonDocument` API (no `StaticJsonDocument`/`DynamicJsonDocument` split needed). |
| `ESP32-A2DP` | Phil Schatzmann (pschatzmann) | ESP32 Audio (`BluetoothAudio`) | Bluetooth Classic A2DP source implementation — this is the library whose own build system enforces "classic ESP32 only" (see `docs/hardware.md` §2.1). Install the latest release from Library Manager, or add `https://github.com/pschatzmann/ESP32-A2DP` as a `.git` dependency if you want a newer unreleased fix. |

## Built into the ESP32 Arduino core (no separate install needed)

These ship with the `esp32` board package itself — do **not** search for
them separately in Library Manager, they'll just show as "already
installed" or won't appear at all because they're core headers:

- `esp_camera.h` (via the core's bundled `esp32-camera` component) — used by `CameraManager`
- `SD.h` / `FS.h` — used by `SDManager`
- `WiFi.h` — used by `WiFiManager` (XIAO) and `WiFiAudio` (ESP32 Audio)
- `ESP_I2S.h` — used by `AudioManager`'s mic capture (XIAO only)

## PSRAM setting (XIAO only)

**Tools → PSRAM → OPI PSRAM** — required. The camera driver in
`CameraManager.cpp` allocates frame buffers in PSRAM at `UXGA` resolution;
without PSRAM enabled, `esp_camera_init()` will fail or you'll be forced to
a much smaller frame size than intended.

## Partition scheme (XIAO only)

**Tools → Partition Scheme → Default (or "Huge APP" if you enable
`LanguageAI`/`VisionAI` experimentation later and start running low on flash
for program storage).** The v1 firmware as shipped fits comfortably in the
default scheme.

## v1.1 / v1.1.1 additions — no new dependencies

`ButtonManager`, `AudioBackend`/`AudioBackendManager`, `STTEngine`, and the
restructured `WakeWordEngine`/`CommandManager` added in v1.1 use only
`Arduino.h` core functions (`pinMode`, `digitalRead`, `millis()`) and the
same `ArduinoJson`/`WiFi.h` already listed above. `CommandManager::
doRecordAudio()` and `doSetVolume()` (v1.1.1) reuse `AudioManager` and
`WiFiManager`/`ConfigManager`, already present — no new Library Manager
entry is needed for any of this.

## What was deliberately NOT added as a dependency

- **No MP3/AAC decoder library.** Music playback expects mono 16-bit PCM WAV
  files (see `docs/architecture.md`) specifically so v1 doesn't need one.
  Convert your music with any standard tool (e.g. `ffmpeg -i song.mp3 -ar
  16000 -ac 1 -sample_fmt s16 song.wav`) before copying it to the SD card.
- **No wake-word / speech-recognition library** (e.g. ESP-SR, Picovoice). See
  `docs/architecture.md` for why — the interface is ready, nothing is
  faked. (Note: as of Aug 2026, Espressif's `esp-sr` does ship a pretrained
  "Jarvis" WakeNet9 model for ESP32-S3 — see that doc for what that does
  and doesn't change about the integration effort.)
- **No on-device vision/LLM library.** Same reasoning, see
  `docs/architecture.md`.

## v1.2 AI Offline — two OPTIONAL, EXPERIMENTAL dependencies (neither installed by default)

Both are off unless you deliberately opt in — see
`docs/architecture.md` "Part 1"/"Part 2" and `docs/installation.md`
"Enabling MultiNet (experimental)" / "Enabling Edge Impulse vision
(experimental)" before touching either.

| Dependency | Used by | Install | Notes |
|---|---|---|---|
| `ESP_SR` (+ `ESP_I2S`) | `MultiNetSTT.h/.cpp`, `examples/MultiNet_Test` | Ships INSIDE the `esp32` board package itself (Espressif) — no separate Library Manager install. Requires a recent `esp32` core (3.x) so `Examples > ESP-SR > Basic` appears at all. | ESP32-S3 only. Needs `OVE_ENABLE_MULTINET` defined (see top of `XIAO_OpenVisionEye.ino`) AND a Partition Scheme with a reserved MultiNet model partition — **not confirmed to exist for the `XIAO_ESP32S3` board entry at 8MB flash**, see `docs/architecture.md`. |
| Your own exported Edge Impulse Arduino library | `VisionAI.h`'s `EdgeImpulseVisionAI`, `examples/Vision_Test` | Edge Impulse Studio -> Deployment -> "Arduino library" -> Build -> Sketch > Include Library > Add .ZIP Library | Confirmed to build/run on this exact board by multiple independent sources (see `docs/architecture.md` "Part 2"). There is no generic file to install — it is generated from YOUR OWN trained model, there is nothing this project can ship in advance. Needs `OVE_ENABLE_EDGE_IMPULSE` defined. |

