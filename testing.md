# OpenVisionEye v1.1.1-button-voice — Test Checklist

None of these have been run against real hardware in this environment —
there is no ESP32-S3/ESP32 Classic, camera, mic, SD card, or Bluetooth
headset attached to the sandbox this project was edited in, and no Arduino
CLI toolchain either (see `HARDWARE_STATUS.md` "Compilation" for what static
checking was actually done instead). Every test below is marked
**NOT VERIFIED** for that reason — this file is a checklist for *you* to run
once the boards arrive, not a report of things that were tested.

| # | Test | How to run it | Expected result | Status |
|---|---|---|---|---|
| 1 | Boot | Flash `XIAO_OpenVisionEye.ino`, open Serial Monitor @115200 | `OpenVisionEye starting...` then the diagnostic block | NOT VERIFIED |
| 2 | PSRAM | Same boot log | `[OK] Camera` (camera init fails fast without PSRAM — see `CameraManager.cpp`) | NOT VERIFIED |
| 3 | Camera | Same boot log, or `examples/XIAO_camera_only` standalone sketch | `[OK] Camera`; standalone example takes a photo on BOOT press | NOT VERIFIED |
| 4 | SD | Same boot log | `[OK] SD`; `/OpenVisionEye/...` folder tree created — see `SDManager::ensureDirectoryStructure()` | NOT VERIFIED |
| 5 | Microphone | Same boot log | `[OK] Microphone` | NOT VERIFIED |
| 6 | Wi-Fi | Same boot log, then check a phone/laptop can see the AP | `[OK] Wi-Fi AP`, SSID `OpenVisionEye-XXXXXX` visible | NOT VERIFIED |
| 7 | PING/PONG | Type `PING_TEST` on the XIAO's Serial Monitor once the Audio hub is connected | XIAO logs a round-trip time in ms — see `docs/wifi_protocol.md` | NOT VERIFIED |
| 8 | 440 Hz audio | Type `AUDIO_TEST` on the XIAO's Serial Monitor | A 1-second 440Hz tone through your Bluetooth headphones | NOT VERIFIED |
| 9 | Bluetooth A2DP | Flash `ESP32_Audio.ino`, set `HEADSET_BT_NAME`, power on | Headphones pair; `ESP32-A2DP` library logs a connected state — see `docs/installation.md` §9 | NOT VERIFIED |
| 10 | Button single click → PHOTO | Wire the button (GPIO2/"D1"→GND), single-click it | `[Button] click=... -> action=PHOTO`, new file in `/OpenVisionEye/photos/` | NOT VERIFIED |
| 11 | Button double click → VIDEO TOGGLE | Double-click the button, wait, double-click again | Video starts, then stops; `.mjpg`-style file in `/OpenVisionEye/videos/` — see `docs/architecture.md` for the real (non-MP4) format | NOT VERIFIED |
| 12 | Button long press → COMMAND MODE | Long-press (>800ms) the button | `[CommandManager] (button) command mode — listening for a command...` on Serial | NOT VERIFIED |
| 13 | Typed command → CommandManager → PHOTO | Long-press the button (or type `HEY_GLASSES`), then type `take a photo` | `[SAY] Photo taken.`, new file in `/OpenVisionEye/photos/` | NOT VERIFIED |
| 14 | Typed command → CommandManager → BATTERY | Same listening window, then type `battery` | `[SAY] Battery status is not available. See docs/hardware.md.` (or a real percentage if you wired the divider — see `docs/hardware.md` §1.6) | NOT VERIFIED |
| 15 | `say()` (TTS interface) → Audio hub | Trigger any command that calls `CommandManager::say()` (e.g. test 13) | `[SAY] ...` is logged on **both** the XIAO and, once forwarded, the Audio hub's own Serial — no audio is produced, since TTS is NOT-IMPLEMENTED. This confirms the interface/plumbing, not spoken output | NOT VERIFIED |

## Extra tests added in v1.1.1 (not in the original numbered list, but worth running)

| Test | How to run it | Expected result | Status |
|---|---|---|---|
| "record audio" command | Open the listening window, type `record audio` | Blocks ~5s, then `[SAY] Audio saved.` and a new `REC_NNNNNN.wav` in `/OpenVisionEye/audio/` | NOT VERIFIED |
| "volume \<n\>" command | Open the listening window, type `volume 40` | `[SAY] Volume set to 40.` on the XIAO, and a `setVolume()` log line on the **Audio hub's** own Serial | NOT VERIFIED |
| `wakeWordEnabled: false` default | Fresh `config.json`, press BOOT | Nothing happens (no "Jarvis" trigger) — confirms the flag is actually gating the auto-trigger, not just stored | NOT VERIFIED |
| `wakeWordEnabled: true` + BOOT press | Edit `config.json`, power-cycle, press BOOT | `[CommandManager] "Jarvis" heard...` — confirms the legacy path still works when explicitly re-enabled | NOT VERIFIED |

## When you do get hardware

Please update the **Status** column in place (`NOT VERIFIED` → `PASS` or
`FAIL` + a short note) rather than deleting rows — a failing, documented
test is more useful to future-you than a silently removed one. If a test
fails, `docs/troubleshooting.md` is organized by symptom and is the first
place to check.
