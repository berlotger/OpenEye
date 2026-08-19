# OpenVisionEye v1.0

DIY smart-glasses firmware project (Ray-Ban Meta–style, custom hardware).
Two independent ESP32-family boards, one per temple, talking over a private
offline Wi-Fi link — no phone, no router, no internet required.

**Read `docs/architecture.md` first.** It has an honest status legend (real
today / needs a library / needs hardware you haven't built yet / experimental
stub / future) for every single module, and explains exactly what is and
isn't realistically achievable on this hardware. Nothing in this project
pretends to work when it doesn't — where something couldn't be verified or
isn't realistic on this chip, that's stated plainly instead of guessed at.

## What's actually real and working in v1

- Camera capture (OV2640/OV3660) → JPEG → microSD, verified pin mapping
- microSD storage with the full folder structure
- Onboard PDM microphone capture, and recording to WAV on SD
- A private offline Wi-Fi link between the two boards (XIAO SoftAP + ESP32
  station), with a documented text/binary protocol
- Music/prompt playback: XIAO reads WAV → streams PCM over Wi-Fi → ESP32
  Audio → Bluetooth Classic A2DP → your headphones
- JSON configuration on SD (language, volume, Wi-Fi AP naming, etc.)
- A honest battery-status pipeline (reports "not available" instead of a
  fake number, until you wire the optional divider circuit)
- A manual wake-word trigger (BOOT button / Serial / TCP text) driving a
  real command dispatcher — not a fake "listens for a keyword" simulation
- A best-effort motion-JPEG "video" recording mode

## What's explicitly NOT real yet (by design, not oversight)

- Spoken wake-word detection (interface ready, see `WakeWordEngine.h`)
- Speech-to-text for full voice commands
- Multi-class on-device vision AI ("person, right, far, dark clothes...")
- A natural-language description model beyond simple templating
- An on-device small LLM (hardware ceiling — 8MB PSRAM/flash total)
- Receiving microphone audio back from your Bluetooth headset

See `docs/architecture.md` for the reasoning behind every one of these, and
what a credible next step looks like.

## Folder structure

```
OpenVisionEye/
├── README.md                     (this file)
├── requirements.md                Exact libraries, versions, board settings
├── docs/
│   ├── architecture.md            Module-by-module status, data flow, design reasoning
│   ├── hardware.md                Verified pins, wiring, hardware requirements
│   ├── wifi_protocol.md           The Wi-Fi protocol between the two boards
│   ├── installation.md            Step-by-step Arduino IDE setup + first test
│   └── troubleshooting.md         Common problems and fixes
│
├── XIAO_OpenVisionEye/            Firmware for the Seeed XIAO ESP32-S3 Sense
│   ├── XIAO_OpenVisionEye.ino
│   ├── ProtocolDefs.h
│   ├── CameraManager.h / .cpp
│   ├── SDManager.h / .cpp
│   ├── WiFiManager.h / .cpp
│   ├── AudioManager.h / .cpp
│   ├── BatteryManager.h / .cpp
│   ├── WakeWordEngine.h / .cpp
│   ├── CommandManager.h / .cpp
│   ├── VisionAI.h / .cpp
│   ├── LanguageAI.h / .cpp
│   └── ConfigManager.h / .cpp
│
├── ESP32_Audio/                   Firmware for the classic-ESP32 audio hub
│   ├── ESP32_Audio.ino
│   ├── ProtocolDefs.h
│   ├── Config.h
│   ├── WiFiAudio.h / .cpp
│   ├── BluetoothAudio.h / .cpp
│   ├── AudioOutput.h / .cpp
│   └── BatteryManager.h / .cpp
│
├── tools/
│   └── send_command.py            Small helper: send a text command over
│                                   the TCP control port from a PC, without
│                                   needing the XIAO's own Serial Monitor.
│
└── examples/                      Minimal single-feature test sketches —
    ├── XIAO_camera_only/          use these to isolate a hardware problem
    └── ESP32_Audio_A2DP_only/     from a firmware/integration problem.
```

## Quick start

1. Read `docs/hardware.md` — confirm your physical wiring matches, especially
   if you're routing the camera to a longer FPC cable for the temple mount.
2. Follow `docs/installation.md` top to bottom.
3. If something doesn't work, check `docs/troubleshooting.md` before
   assuming it's a code bug — several "failures" are expected states (e.g.
   "battery not available" until you wire a circuit).

## Security

- The XIAO's Wi-Fi Access Point uses a local password (`ConfigManager`'s
  `apPassword`, default `glasses1234` — **change this** before relying on it
  for anything beyond bench testing). This is a device-to-device local
  network password, not an internet credential.
- **No internet credentials are stored anywhere in v1** — there is no
  internet connectivity in this version at all, by design (see "Offline
  mode" below and `docs/architecture.md`).
- If you add an internet-connected feature later (see "Future: cloud AI"
  below), do not reuse `ConfigManager`'s plain-JSON storage for any secret —
  add a properly secured storage path (e.g. ESP32's NVS encryption support)
  at that point rather than extending the current plaintext config file.

## Offline mode

The XIAO creates its own Wi-Fi network (`WiFi.softAP()` — see
`WiFiManager.cpp`) and the ESP32 Audio board joins it as a station. Neither
board ever attempts to join an external/internet-connected network in v1.
No phone, no router, no SIM, no internet — confirmed by reading through
every network call in both sketches; there is exactly one `WiFi.mode()` call
per board, and both are the offline modes (`WIFI_AP` / `WIFI_STA` pointed at
each other).

## Future: cloud/external AI (v1.1+)

`VisionAI` and `LanguageAI` are both plain interfaces
(`XIAO_OpenVisionEye/VisionAI.h`, `LanguageAI.h`) precisely so that, once you
want to add internet connectivity, you can implement a new subclass that:

1. Takes the JPEG frame or recorded audio,
2. Sends it to an external API (over Wi-Fi — the XIAO would need a second,
   internet-connected Wi-Fi client mode alongside its SoftAP, or a Wi-Fi
   coexistence approach on the ESP32 Audio side instead, since ESP32 Wi-Fi
   can't simultaneously host an AP and join a different internet AP as a
   station with the same radio in all configurations — evaluate this
   tradeoff when you get there rather than assuming AP+STA "just works" for
   two different networks simultaneously),
3. Returns a `VisionResult` / description string through the exact same
   interface `CommandManager` already calls today.

No other file needs to change to support this — that's the point of the
interface boundary.

## Roadmap

**v1.1 candidates**
- Real wake-word detection via Espressif ESP-SR (WakeNet) or Picovoice
  Porcupine, replacing `ManualTriggerWakeWord`
- Small fixed-vocabulary command recognition (ESP-SR MultiNet) to remove the
  "type your command" step in `docs/installation.md` §10
- A FreeRTOS task for music playback so commands don't block during a song
  (see the known limitation noted in `CommandManager::playCurrentTrack()`)
- A real canned-prompt playback system on the ESP32 Audio side for `SAY:`
  messages (today they're logged, not spoken — see `ESP32_Audio.ino`)
- True pause/resume for music (today `PAUSE` stops the current send loop
  rather than suspending mid-stream — see `CommandManager::doPauseMusic()`)

**v2.0 candidates**
- On-device presence/face detection (`esp-dl`) as a first real `VisionAI`
  implementation
- Internet-backed `VisionAI`/`LanguageAI` implementations (see "Future:
  cloud AI" above)
- "Hey Jarvis" — reserved throughout the codebase (`WakeWord::HEY_JARVIS`)
  for whatever this advanced-assistant hook turns into
- Revisit HFP for headset-microphone input if a real use case justifies its
  complexity (see `docs/hardware.md` §2.4)

## Known limitations (read before you file these as bugs)

- Video recording is a custom `[len][jpeg]`-repeated container, not a
  standard playable video file — see `docs/architecture.md`.
- Music playback blocks the XIAO's main loop for the song's duration —
  see `CommandManager::playCurrentTrack()`.
- Upsampling from 16kHz to 44.1kHz for Bluetooth is simple nearest-sample
  duplication, not a high-quality resampler — see `AudioOutput.h`.
- `Config.h` on the ESP32 Audio board and `ConfigManager.h`/`config.json` on
  the XIAO must be kept in sync manually for the AP SSID prefix and
  password — there's no automatic negotiation in v1.
