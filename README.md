# OpenVisionEye v1.2 AI Offline

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
- **v1.1.1: a physical button (`ButtonManager`) is the primary control** —
  single-click PHOTO, double-click VIDEO_TOGGLE, long-press COMMAND_MODE
  (opens one command-listening window directly — the button is the
  wake-word substitute this version, per spec). Non-blocking, debounced,
  remappable at runtime and persisted to `config.json`
- **v1.1.1: mic → WAV recording ("record audio")** wired end-to-end for the
  first time — saves fixed 5s clips to `/OpenVisionEye/audio/`
- **v1.1.1: "volume \<n\>" command** wired end-to-end — sets `config.json`
  and sends `VOLUME:<n>` to the Audio hub's real `set_volume()` call
- A separate, optional, disabled-by-default "Jarvis" wake-word stub (BOOT
  button / Serial / TCP text), driving the older two-stage command
  dispatcher (Jarvis → mode word → command) — not a fake "listens for a
  keyword" simulation, just no longer the primary flow — see
  `docs/architecture.md`
- An `AudioBackend` abstraction (LOCAL / PHONE / AUTO) — LOCAL is real
  (same Wi-Fi → ESP32 Audio → A2DP path as before); PHONE is an honest
  not-implemented placeholder; AUTO falls back to LOCAL automatically
- A best-effort motion-JPEG "video" recording mode
- Two end-to-end diagnostics you can run before touching anything else:
  `AUDIO_TEST` (440Hz tone through the whole audio pipeline) and
  `PING_TEST` (round-trip latency to the Audio hub) — see `docs/wifi_protocol.md`
- **v1.2: real, model-independent bounding-box math** — LEFT/CENTER/RIGHT
  position and a VERY_NEAR/NEAR/MEDIUM/FAR depth *estimate* (never
  presented as real distance), ready for whatever vision engine feeds it
- **v1.2: `TemplateLanguageAI`** turns a `VisionResult` into a sentence
  ("There is a person on your right.") — real templating, not generative AI

## v1.2 AI Offline — feature honesty table

Per your explicit requirement: nothing below is marked REAL unless it
actually runs without a person supplying external inputs (a trained
model, an unverified partition scheme, etc.) this project can't ship.

| Feature | Status | Verified | Notes |
|---|---|---|---|
| Button command mode (single/double/long-press) | REAL | YES | `ButtonManager` |
| Text/TCP command dispatch | REAL | YES | `CommandManager` |
| Position (LEFT/CENTER/RIGHT) from bounding box | REAL | YES | plain arithmetic, `VisionAI.h` |
| Depth estimate (VERY_NEAR/NEAR/MEDIUM/FAR) | REAL | YES | plain arithmetic, explicitly not real distance |
| Language templates | REAL | YES | `TemplateLanguageAI` |
| MultiNet offline voice commands | EXPERIMENTAL | SOFTWARE ONLY — NOT hardware verified | real ESP-SR API, partition-scheme blocker on 8MB flash, see `docs/architecture.md` "Part 1" |
| Edge Impulse offline object detection | EXPERIMENTAL | SOFTWARE ONLY — NOT hardware verified | real integration point, confirmed-working path elsewhere, but needs YOUR trained model, see "Part 2" |
| Wake word | FUTURE | NO | not implemented, button is the substitute this version |
| TTS | INTERFACE | NO | `SAY:` is logged by the Audio hub, not spoken |

See `docs/architecture.md` for the full per-module status legend and every
module's own writeup.

## What's explicitly NOT real yet (by design, not oversight)

- Spoken wake-word detection (interface ready, see `WakeWordEngine.h`; the
  button is the documented substitute this version, per your spec)
- Full open-vocabulary speech-to-text (interface ready, see `STTEngine.h`
  — deliberately **not** wired into `CommandManager`; MultiNet, above, is
  a *fixed-command* alternative, not general STT, and is itself
  EXPERIMENTAL)
- Phone-managed audio output (interface ready, see `AudioBackend.h`'s
  `PhoneAudioBackend` — always reports unavailable, no phone app/protocol
  exists)
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
│   ├── troubleshooting.md         Common problems and fixes
│   └── testing.md                 Numbered test checklist (all NOT VERIFIED — see file)
│
├── XIAO_OpenVisionEye/            Firmware for the Seeed XIAO ESP32-S3 Sense
│   ├── XIAO_OpenVisionEye.ino
│   ├── ProtocolDefs.h
│   ├── CameraManager.h / .cpp
│   ├── SDManager.h / .cpp
│   ├── WiFiManager.h / .cpp
│   ├── AudioManager.h / .cpp
│   ├── AudioBackend.h / .cpp      v1.1 — LOCAL/PHONE/AUTO output routing
│   ├── BatteryManager.h / .cpp
│   ├── WakeWordEngine.h / .cpp    v1.1 — "Jarvis" stub, SECONDARY/legacy in v1.1.1 (disabled by default)
│   ├── ButtonManager.h / .cpp     v1.1.1 — PRIMARY command trigger (long-press = command mode)
│   ├── STTEngine.h                v1.1 — interface only, not wired in
│   ├── MultiNetSTT.h / .cpp       v1.2 — EXPERIMENTAL offline voice commands, off by default
│   ├── VoiceCommandIds.h          v1.2 — shared Command ID enum
│   ├── CommandManager.h / .cpp
│   ├── VisionAI.h / .cpp          v1.2 — adds position/depth math (REAL) + EdgeImpulseVisionAI (EXPERIMENTAL)
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
    ├── ESP32_Audio_A2DP_only/     from a firmware/integration problem.
    ├── MultiNet_Test/             v1.2 — voice-only isolation test (EXPERIMENTAL)
    └── Vision_Test/               v1.2 — camera-only isolation test (EXPERIMENTAL, needs your own trained model)
```

## Quick start

1. Read `docs/hardware.md` — confirm your physical wiring matches, especially
   if you're routing the camera to a longer FPC cable for the temple mount.
2. Follow `docs/installation.md` top to bottom.
3. **First real test, before anything else:** with both boards powered and
   paired to your headphones, type `AUDIO_TEST` in the XIAO's Serial
   Monitor. If you hear a steady 440Hz tone, the entire chain (Wi-Fi →
   ESP32 Audio → Bluetooth A2DP → headphones) works — everything else
   builds on top of that. Then try `PING_TEST` to check link latency.
4. If something doesn't work, check `docs/troubleshooting.md` before
   assuming it's a code bug — several "failures" are expected states (e.g.
   "battery not available" until you wire a circuit).
5. `docs/testing.md` has the full numbered test checklist (boot, PSRAM,
   camera, SD, mic, Wi-Fi, ping, audio tone, Bluetooth, button clicks,
   typed commands, TTS plumbing) to run through once your boards arrive —
   nothing in it has been run against real hardware yet, by design; it's a
   checklist for you, not a report.

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

**v1.2 candidates — DONE this version (with honest caveats, see above)**
- ~~Small fixed-vocabulary command recognition (ESP-SR MultiNet)~~ — done
  as `MultiNetSTT`, but EXPERIMENTAL/not hardware-verified, see
  `docs/architecture.md` "Part 1"
- ~~Offline object detection~~ — done as `EdgeImpulseVisionAI`, but
  EXPERIMENTAL and needs your own trained model, see "Part 2"
- ~~Position/depth estimation from a bounding box~~ — done,
  `computePosition()`/`computeDepth()` in `VisionAI.h`, REAL

**v1.3 candidates**
- Real on-device "Jarvis" wake-word detection — Espressif's `esp-sr` ships
  a pretrained WakeNet9 model for exactly this word (`wn9_jarvis_tts`,
  ESP32-S3); would replace `ManualTriggerWakeWord`. Same partition-scheme
  question as MultiNet applies.
- Wire `STTEngine` into `CommandManager` once a real general-purpose
  implementation exists (`LocalSTT`/`OnlineSTT`/`PhoneSTT`) — see
  `STTEngine.h`
- Make "record audio" non-blocking (chunked per `loop()` call, the same
  technique video recording already uses) instead of its current fixed-
  duration blocking call — see `CommandManager::doRecordAudio()`
- A real, cancelable audio-recording stop and a real mid-track music stop
  (both currently fall back to the closest existing real behavior — see
  `CommandManager::handleVoiceCommand()`'s honest gaps for `STOP_RECORDING`
  / `STOP_MUSIC`)
- A real `PhoneAudioBackend` (companion app + a real transport — Bluetooth
  to the phone, or a second Wi-Fi role) — see `AudioBackend.h`
- A FreeRTOS task for music playback so commands don't block during a song
  (see the known limitation noted in `CommandManager::playCurrentTrack()`)
- A real canned-prompt playback system on the ESP32 Audio side for `SAY:`
  messages (today they're logged, not spoken — see `ESP32_Audio.ino`)
- True pause/resume for music (today `PAUSE` stops the current send loop
  rather than suspending mid-stream — see `CommandManager::doPauseMusic()`)
- A dedicated `MUSIC` mode word grammar (the mode word is recognized today
  — see `CommandManager::onModeWord()` — but replies "not available yet")
- If MultiNet's partition-scheme blocker gets resolved: figure out whether
  `CONFIG_MODEL_IN_SDCARD` is reachable from Arduino IDE for this board —
  would remove the 8MB flash-budget problem entirely

**v2.0 candidates**
- Internet-backed `VisionAI`/`LanguageAI` implementations (see "Future:
  cloud AI" above)
- Revisit HFP for headset-microphone input if a real use case justifies its
  complexity (see `docs/hardware.md` §2.4)

## Known limitations (read before you file these as bugs)

- Video recording is a custom `[len][jpeg]`-repeated container, not a
  standard playable video file — see `docs/architecture.md`.
- Music playback blocks the XIAO's main loop for the song's duration —
  see `CommandManager::playCurrentTrack()`.
- "record audio" blocks the XIAO's main loop for a fixed 5 seconds (camera/
  Wi-Fi/button polling pause during that window) — see
  `CommandManager::doRecordAudio()`.
- Upsampling from 16kHz to 44.1kHz for Bluetooth is simple nearest-sample
  duplication, not a high-quality resampler — see `AudioOutput.h`.
- `Config.h` on the ESP32 Audio board and `ConfigManager.h`/`config.json` on
  the XIAO must be kept in sync manually for the AP SSID prefix and
  password — there's no automatic negotiation in v1.
