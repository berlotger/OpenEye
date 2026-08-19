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

## What was deliberately NOT added as a dependency

- **No MP3/AAC decoder library.** Music playback expects mono 16-bit PCM WAV
  files (see `docs/architecture.md`) specifically so v1 doesn't need one.
  Convert your music with any standard tool (e.g. `ffmpeg -i song.mp3 -ar
  16000 -ac 1 -sample_fmt s16 song.wav`) before copying it to the SD card.
- **No wake-word / speech-recognition library** (e.g. ESP-SR, Picovoice). See
  `docs/architecture.md` for why — the interface is ready, nothing is faked.
- **No on-device vision/LLM library.** Same reasoning, see
  `docs/architecture.md`.
