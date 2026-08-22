# OpenVisionEye — Software Architecture

**Current version: v1.2 AI Offline** (see README.md for the full version history)

## Implementation status legend

Every module in this project is labeled with one of these, both here and in its
own header comment, per your requirement to never pretend something works that
doesn't:

1. **REAL — works today** on the listed hardware, no extra hardware needed.
2. **REAL — needs a library** — works today, depends on a third-party library
   (listed in `requirements.md`).
3. **NEEDS EXTRA HARDWARE** — the interface and call sites exist; the concrete
   implementation needs hardware you haven't fixed yet (e.g. a battery divider
   circuit).
4. **EXPERIMENTAL** — a real implementation exists but is unreliable / a stub you
   should expect to replace (e.g. wake word).
5. **FUTURE (v2.0+)** — interface only, intentionally not implemented, because it
   isn't realistically achievable on this hardware today (e.g. an on-device LLM).

## Module map — XIAO (patilla dreta / right temple)

| Module | Status | Notes |
|---|---|---|
| `CameraManager` | 1 REAL | esp_camera + OV2640/OV3660, photo + best-effort MJPEG "video" |
| `SDManager` | 1 REAL | SD.begin(21), folder structure, file helpers |
| `WiFiManager` | 1 REAL | SoftAP + the two TCP servers from `wifi_protocol.md` |
| `AudioManager` | mixed | mic capture: 1 REAL (ESP_I2S). Music/TTS playback: **routed to the ESP32 Audio board** (XIAO has no speaker/DAC in this BOM) — see below |
| `BatteryManager` | 3 NEEDS HARDWARE | interface + disabled-by-default ADC implementation, see `hardware.md` §1.6 |
| `WakeWordEngine` | 4 EXPERIMENTAL | interface + a manual-trigger stub (button/Serial/TCP), see below |
| `ButtonManager` | 1 REAL | v1.1.1 — **primary command trigger** this version (spec §7/§8), non-blocking single/double/long click, configurable mapping |
| `AudioBackend` | mixed | v1.1 — `LocalAudioBackend` 1 REAL, `PhoneAudioBackend` 5 FUTURE, see below |
| `STTEngine` | 5 FUTURE | v1.1 — interface only, not wired into `CommandManager`, see `STTEngine.h` |
| `MultiNetSTT` | 4 EXPERIMENTAL | v1.2 — real ESP-SR API usage, off by default (`OVE_ENABLE_MULTINET`), NOT confirmed to boot on this board's 8MB flash — see "Part 1" below |
| `CommandManager` | 1 REAL | dispatches text commands from Serial / stub wake-word / TCP / button / (optionally) MultiNet to the other modules |
| `VisionAI` | mixed | `NullVisionAI` (default) 1 REAL; `EdgeImpulseVisionAI` (v1.2, `OVE_ENABLE_EDGE_IMPULSE`) 4 EXPERIMENTAL — needs YOUR trained model, see "Part 2" below; position/depth math is 1 REAL either way |
| `LanguageAI` | 1 REAL (trivial) | `TemplateLanguageAI` — see below |
| `ConfigManager` | 1 REAL | JSON config file on SD via ArduinoJson |

## Module map — ESP32 Audio (patilla esquerra / left temple)

| Module | Status | Notes |
|---|---|---|
| `WiFiAudio` | 1 REAL | Wi-Fi station, connects to XIAO's control+audio TCP servers |
| `BluetoothAudio` | 2 REAL (needs lib) | `ESP32-A2DP` library, `BluetoothA2DPSource` |
| `AudioOutput` | 1 REAL | buffers PCM received over Wi-Fi and feeds it to the A2DP callback |
| `BatteryManager` | 3 NEEDS HARDWARE | same situation as XIAO's |

## v1.1.1-button-voice: wake word is FUTURE, the button is the trigger

Per your spec (§7/§8): **this version does not implement wake-word
detection.** The physical button's long-press is the documented substitute
— it opens the same command-listening window a real "Hey Jarvis" would have
opened, without any audio keyword spotting involved. `ConfigManager`'s
`wakeWordEnabled` defaults to **false**, so the separate "Jarvis" stub
described below doesn't even poll the BOOT button unless you explicitly
re-enable it — see `ConfigManager.h` and `CommandManager::loop()`.

## Why wake word is a stub (legacy/optional path, not the primary trigger)

You explicitly asked not to fake this with a naive string search, so here is
the honest state of the art as of this project — including specifically about
"Jarvis", since a prior revision (v1.1) named the wake word that.

- **Is there a "native ESP32 wake word" you can just drop in?** As of this
  revision (checked Aug 2026), the picture has genuinely changed since the
  claim this section used to make, and the correction matters:
  - Espressif's own **ESP-SR** (WakeNet + MultiNet) now officially lists a
    pretrained **"Jarvis" WakeNet9 model (`wn9_jarvis_tts`)** for ESP32-S3 in
    its component registry (espressif/esp-sr, v1.9.0+/master) — this
    project's earlier claim that no native/pretrained "Jarvis" model exists
    is **no longer accurate** and is corrected here rather than left to
    stand. This does not mean it's a one-line Arduino library call, though:
    ESP-SR ships as an **ESP-IDF component**, not an Arduino Library
    Manager package. Using it from an Arduino IDE sketch means going
    through Arduino-ESP32's "Arduino as an ESP-IDF component" integration
    path (symlinking the Arduino core into an ESP-IDF project, wiring
    `idf_component.yml`, etc.) — real, documented, but a genuinely different
    and more involved build workflow than everything else in this project,
    which is why it's still not done here, on top of you explicitly asking
    not to build wake word this version.
  - Espressif's **MultiNet** (offline command-word recognition, the same
    component family) also has English models for ESP32-S3
    (`mn5q8_en`/`mn6_en`/`mn7_en`) — relevant for a future STT-adjacent
    step (recognizing a small fixed vocabulary like "photo"/"video"/
    "battery" from speech), see `STTEngine.h` and the roadmap below.
  - **openWakeWord** (dscripka) ships a pretrained `hey_jarvis` model, but
    its embedding model is too slow to run on an ESP32 — it's meant to run
    on a PC/server that a satellite device streams audio to. Still not a
    fit for this hardware.
  - **microWakeWord** (ESPHome's `micro_wake_word`, kahrendt/esphome-on-
    device-wake-word) also has its own on-device "Hey Jarvis" TFLite Micro
    model and remains a real alternative — built as an ESPHome C++
    component, so porting it into this bare-Arduino sketch is still a real,
    separate subproject, same as before.
  - Given all of that, `WakeWordEngine` stays a clean interface (`begin()`,
    `poll()` returning which wake word if any) with the same honest shipped
    stub as before: `ManualTriggerWakeWord`, triggered by any press/release
    of the **BOOT button** (now gated behind `wakeWordEnabled`, default
    off) or by sending the literal string `JARVIS` (or `HEY_JARVIS`) over
    Serial or the diagnostic TCP port (always available, regardless of the
    flag — see `CommandManager::handleIncomingText()`).
- **Two-stage "Jarvis" flow (still present, now secondary):** "Jarvis" opens
  a mode-select window; the next word ("glasses"/"ai"/"music") opens a
  second command/question window. This is real, working code — kept for
  anyone who wants it — but it is not what `docs/installation.md`'s first
  test walks you through anymore; the button (or its `HEY_GLASSES` bench
  stand-in) is, per your spec.
- To add real wake-word detection later: implement `WakeWordEngine` with
  ESP-SR (now with a pretrained "Jarvis" model available, see above),
  microWakeWord, or a custom-trained model, and swap it in
  `CommandManager::begin()`. No other file needs to change —
  `CommandManager` only ever sees `WakeWord::JARVIS`, regardless of how it
  was detected.

## ButtonManager (v1.1.1) — the primary command trigger

The physical button (see `ButtonManager.h` for the GPIO2/"D1" choice and
why — `BUTTON_GPIO = 2`) is how you actually drive this version day to day:
single-click PHOTO, double-click VIDEO_TOGGLE, long-press COMMAND_MODE
(spec §10/§11). Long-press is the documented wake-word substitute (spec
§8) — it opens ONE command-listening window directly, no mode word needed.
Detection is a genuine non-blocking `millis()`-based state machine (single/
double/long click, with debounce) — no `delay()` calls anywhere in it, so it
can't stall the camera/audio/Wi-Fi loop. No button wired yet? `HEY_GLASSES`
typed on Serial/TCP opens the identical listening window.

The mapping (which action fires on single/double/long click) is stored in
`config.json` and changeable at runtime, without reflashing, via
`BUTTON_SINGLE:<ACTION>` / `BUTTON_DOUBLE:<ACTION>` / `BUTTON_LONG:<ACTION>`
text commands over Serial or the TCP control port (`<ACTION>` is one of
`PHOTO`, `VIDEO_TOGGLE`, `COMMAND_MODE`, `NONE`) — see
`docs/wifi_protocol.md`.

## AudioBackend: LOCAL / PHONE / AUTO (v1.1)

Per your requirement, outgoing PCM audio (music, `SAY:` prompts, the
`AUDIO_TEST` tone) now goes through an `AudioBackend` abstraction instead of
calling `WiFiManager::sendAudioChunk()` directly:

- **LOCAL** — 1 REAL. Exactly what v1.0 always did: XIAO → Wi-Fi → ESP32
  Classic Audio hub → A2DP → headphones.
- **PHONE** — 5 FUTURE / NOT IMPLEMENTED. There is no phone app, no pairing
  mechanism, and no protocol for this in the codebase. `PhoneAudioBackend`
  always reports itself unavailable and always fails `sendPcm()` — it exists
  purely so `AUDIO_MODE` has somewhere real to point once phone audio is
  actually built. **Do not** treat log lines mentioning "PHONE" as a working
  feature.
- **AUTO** — resolves to PHONE if it's ever available, otherwise LOCAL.
  Since PHONE is never available in v1.1, AUTO behaves identically to LOCAL
  today — but re-resolves on every audio chunk (not cached), so the
  fallback-to-LOCAL behavior you asked for is already correct and will keep
  working once PHONE becomes real, with no changes needed elsewhere.

Set with `audioBackendMode` in `config.json` (`"AUTO"`, `"LOCAL"`, or
`"PHONE"` — default `"AUTO"`).

## STTEngine (v1.1) — interface only, deliberately not wired in

`STTEngine.h` defines the interface your spec asked for (`LocalSTT` /
`OnlineSTT` / `PhoneSTT` would implement it later) plus a `MockSTT` test
double. It is **not** referenced from `CommandManager` or the main `.ino`.
Reason: `CommandManager` already gets its command text from a human typing
into Serial/TCP, which *is*, functionally, manual/mock STT. Wrapping that in
an `STTEngine`-shaped class and calling it "wired in" would look like more
integration exists than it actually does. See the comment at the top of
`STTEngine.h` for the full reasoning.

## Part 1 — offline voice command recognition (v1.2 AI Offline)

Your spec asked specifically for Espressif ESP-SR MultiNet, with a hard
requirement to verify rather than assume. Here is what was actually
checked (Aug 2026), against official/primary sources, not blog posts:

**Is there a real Arduino path at all?** Yes — this is a genuine update
versus how this file used to describe ESP-SR. `arduino-esp32` (the core
itself) now ships a built-in `ESP_SR` Arduino library
(`arduino-esp32/libraries/ESP_SR`), with a real `Examples > ESP-SR > Basic`
sketch. You `#include "ESP_SR.h"` from a normal `.ino` — this is NOT the
"must integrate ESP-IDF as a component" situation the wake-word section
above describes for `microWakeWord`/raw `esp-sr`.

**What's confirmed:**
- ESP32-S3 only (`CONFIG_IDF_TARGET_ESP32S3`) — matches the XIAO.
- MultiNet's expected audio format is 16 kHz / 16-bit / mono — identical
  to this project's existing `AudioManager::SAMPLE_RATE`, no reprocessing
  needed.
- English MultiNet models exist (`mn5q8_en`/`mn6_en`/`mn7_en`,
  Espressif's own `esp-sr` repo) supporting up to ~200-300 command
  phrases — comfortably enough for the 10 commands in your spec.

**What's genuinely NOT confirmed, and is the real blocker:**
- The `ESP_SR` library's own source (`esp32-hal-sr.c`) contains
  `#warning Compatible partition must be selected for ESP_SR to work`,
  gated on `ARDUINO_PARTITION_esp_sr_8` / `_16` / `_32` build flags — i.e.
  it needs a Tools > Partition Scheme entry reserving a MultiNet model
  partition. Vendor examples (e.g. Waveshare's ESP32-S3 boards) show this
  as something like "ESP SR 16M (3MB APP/7MB SPIFFS/2.9MB MODEL)" — sized
  for **16 MB** flash. The XIAO ESP32S3 Sense has **8 MB**. Whether an
  `esp_sr_8` scheme is actually exposed for the `XIAO_ESP32S3` board entry
  in Boards Manager, and whether it leaves enough room once your own
  sketch is added, is unverified.
- The same source also gates on `CONFIG_MODEL_IN_SDCARD` as an alternative
  to `CONFIG_MODEL_IN_FLASH` — meaning the underlying `esp-sr` component
  DOES support loading the model from an SD card instead of a flash
  partition, which would sidestep the 8 MB problem entirely (this project
  already has an SD card). But there's no confirmed, documented way to
  select that from the Arduino IDE Tools menu for this board — Arduino IDE
  only exposes whatever menu options `boards.txt` defines, and this option
  was not found there for `XIAO_ESP32S3`. This is the single biggest open
  question for whoever picks this up next — not something to fake past.
- A dated, reproducible hardware report (Arduino Forum, March 2026) shows
  someone running the **exact official Basic example**, board =
  `XIAO_ESP32S3(_PLUS)` (this board family), crashing at boot:
  `E MODEL_LOADER: Can not find model in partition table` followed by a
  memory-exhausted panic — before any user code runs. Related threads
  ("Errno2 looking for srmodels.bin", "ESP-SR compile error", "ESP_SR
  works ... sometimes") span roughly Jan 2025 - Apr 2026, suggesting this
  is a recurring friction point, not a one-off.

**What was actually implemented:** `MultiNetSTT.h/.cpp` — real calls
against the real `ESP_SR` API (`sr_cmd_t`, `ESP_SR.begin()`,
`SR_EVENT_COMMAND`, etc., the same shapes the official example uses), and
`examples/MultiNet_Test/` as the isolated test sketch your spec asked for.
Neither is fake, and neither is proven — both are compiled out by default
(`OVE_ENABLE_MULTINET` undefined) and `VOICE_MODE` defaults to `OFF` in
`config.json`. See `MultiNetSTT.h` for the full, itemized honesty writeup.
**"Not hardware verified" — you are the first person to actually try
compiling this against real hardware.**

## Part 2 — offline object detection (v1.2 AI Offline)

Your spec asked for Edge Impulse as an Arduino library. Unlike Part 1
above, this path IS confirmed working on this exact board:

- Edge Impulse's own hardware docs list "Seeed XIAO ESP32S3 Sense" as a
  supported target.
- Multiple independent, hands-on sources (Seeed's own engineering blog;
  Marcelo Rovai's widely-cited "TinyML Made Easy" Hackster.io series;
  several Edge Impulse forum threads) show a FOMO (Faster Objects, More
  Objects) model trained in Edge Impulse Studio, exported as an Arduino
  `.zip` library, and run on this exact board with its OV2640 camera —
  reporting concrete numbers (~140 ms inference, ~7 fps at low resolution,
  PSRAM-backed frame buffers).
- Deployment is "Sketch > Include Library > Add .ZIP Library" — no
  ESP-IDF, no custom partition scheme needed for this part (materially
  different situation from MultiNet above).
- FOMO doesn't output a bounding-box *size* the way MobileNet-SSD/YOLO do
  (per Edge Impulse's own docs it reports a centroid-style detection) —
  worth knowing before assuming the box math below gets identical
  precision across every model type you might train.

**So why does `EdgeImpulseVisionAI` still ship as EXPERIMENTAL, not REAL?**
Because a trained model is not generic software — it's weights derived
from YOUR photos. There is no universal `person`/`shoe`/`bottle` detector
file this project can ship; every Edge Impulse deployment starts from your
own Studio project. `VisionAI.h`'s `EdgeImpulseVisionAI` is a real,
correctly-shaped integration point (calls the exported library's actual
`run_classifier()`/`EI_CLASSIFIER_INPUT_WIDTH` API), and
`examples/Vision_Test/` is the isolated test sketch — but both are inert
until you complete `docs/installation.md` "Enabling Edge Impulse vision
(experimental)" and drop in your own exported header. That is a "not
hardware verified, and can't be verified by anyone but you" gap, not a
code gap — see the full writeup in `VisionAI.h`.

**Position (LEFT/CENTER/RIGHT) and depth (VERY_NEAR/NEAR/MEDIUM/FAR)** are
real, model-independent arithmetic on whatever bounding box a detector
returns — `computePosition()`/`computeDepth()` in `VisionAI.h` — per your
spec: three equal horizontal thirds of the frame for position, and a
bucketed bounding-box-area ratio for the depth *estimate* (explicitly
never presented as meters — single camera, no stereo/depth sensor in this
BOM).

## Arduino IDE vs. ESP-IDF — the three options from your spec

- **Option A (Arduino IDE + MultiNet + Edge Impulse together): Likely
  viable, not fully verified.** Edge Impulse's Arduino path has no special
  build requirements beyond what this project already uses. MultiNet's
  partition-table requirement (Part 1 above) is the only real friction —
  if you find/create a working 8MB "esp_sr" partition scheme, both should
  coexist fine (they don't compete for the same flash region; MultiNet
  needs a model partition, Edge Impulse's model just links into your
  sketch's own app partition). Not verified end-to-end on real hardware.
- **Option B (Arduino IDE + only MultiNet): Same blocker as Option A**,
  minus Edge Impulse's flash-budget contribution. Doesn't remove the core
  open question (8MB partition scheme / SD-card model loading).
- **Option C (migrate to ESP-IDF): Not necessary based on what was
  found.** Edge Impulse needs no ESP-IDF migration on this board at all.
  Even MultiNet's blocker is an Arduino **partition-scheme** and
  **Tools-menu-exposure** question, not a fundamental "Arduino can't do
  this" limitation — the underlying `esp-sr` component itself works from
  ESP-IDF OR from Arduino via the built-in wrapper; migrating everything
  to ESP-IDF would trade one set of problems (partition schemes) for a
  much larger one (rewriting `CameraManager`/`SDManager`/`WiFiManager`/the
  A2DP audio hub, none of which need to change for either AI path). **Stay
  on Arduino IDE.**



### Older reality-check notes (multi-class detection, depth, color)

This is the original v1.1 analysis, kept for context — Part 2 above is the
current v1.2 status:

- **Multi-class detection** (person / animal / vehicle / etc.,
  simultaneously, in real time) at high accuracy is still not realistic on
  this chip's compute/RAM budget — FOMO's per-class approach (Part 2) is
  the practical middle ground, not full-accuracy general object detection.
- **Color naming and "young-looking" style attributes** from a single 2D
  camera still have no reliable on-device solution — hence `VisionResult`
  still carries `color`/`size` as legacy, always-empty fields (see
  `VisionAI.h`).
- **Depth** is now implemented as an explicit *estimate* (Part 2's
  `computeDepth()`), not real distance — this supersedes the earlier
  "no reliable solution at all" conclusion now that the bucketed-estimate
  approach was implemented and documented as an estimate everywhere it's
  shown.

`VisionAI` ships with `NullVisionAI` (default, `VISION_MODE=OFF`) which
honestly returns "not available", plus the `EdgeImpulseVisionAI`
integration point (Part 2, `VISION_MODE=LOCAL`) for once you've trained
your own model.

## Language AI ("second layer") reality check

Turning structured vision output into a natural sentence is a text-generation
task. A small local text model is not realistic in the RAM available (see
`docs/hardware.md` and the note in `LanguageAI.h`) for anything beyond templated
string filling. `LanguageAI` therefore ships as an interface with a
`TemplateLanguageAI` implementation that does honest templated sentences (e.g.
`"{color} {size} {label}, {position}, {depth}."` → filled from whatever
`VisionAI` actually returns), so the pipe is real end-to-end even though today's
`VisionAI` doesn't return much to fill in. A future local, mobile, or
server/API-backed implementation slots into the same interface.

## On-device small LLM ("IA de text")

Investigated and **not implemented, and not recommended** for this hardware: the
XIAO ESP32-S3 has 8 MB PSRAM / 8 MB flash total. Even the smallest usable
quantized instruction-following LLMs need tens to low-hundreds of MB for weights
plus a KV cache, one to two orders of magnitude more than what's physically on
this board — so this is a hardware ceiling, not a missing library. The interface
(`LanguageAI`, above) is ready to receive a future implementation that talks to a
phone/server/API instead.

## Audio: how music/TTS actually gets to your ears

The XIAO has a mic (real, PDM) but **no on-board speaker or audio DAC**, and
Bluetooth Classic doesn't run on the ESP32-S3 anyway. So playback is architected
as: XIAO reads a file from `/OpenVisionEye/music/` or `/OpenVisionEye/audio/` off
its SD card → streams raw PCM to the ESP32 Audio board over the Wi-Fi audio
channel (`wifi_protocol.md`) → ESP32 Audio forwards those samples to
`BluetoothA2DPSource`'s callback → your Bluetooth headphones. `AudioManager` on
the XIAO side is the file reader/streamer; `AudioOutput` on the ESP32 Audio side
is the receive-buffer/A2DP-feed. Recommended source format: **mono 16-bit PCM
WAV at 16000 Hz**, because that avoids needing an on-device MP3/AAC decoder at
all (see `requirements.md` for why MP3 decoding was intentionally left out of
v1).

## AudioInput abstraction (mic source selection)

Per your requirement, mic source is not hardcoded:

```cpp
enum class AudioInputSource { MIC_XIAO, MIC_HEADSET, MIC_EXTERNAL };
```

`MIC_XIAO` is the only one implemented in v1 (the onboard PDM mic). `MIC_HEADSET`
is defined but **not implemented** — see `hardware.md` §2.4 for why (A2DP has no
return audio path; would need HFP). `MIC_EXTERNAL` is a placeholder for a future
wired mic on a free GPIO.

## Data flow — "long-press, take a photo" (v1.1.1, PRIMARY flow)

```
Button long-press (or Serial/TCP "HEY_GLASSES")   [button = wake-word substitute, spec §8]
        │
        ▼
ButtonManager::poll() → ButtonClick::LONG_PRESS → ButtonAction::COMMAND_MODE
        │
        ▼
CommandManager::handleButtonAction() → Stage::GLASSES_COMMAND (8s window)
        │  next text line (typed today — a real STT engine plugs in here
        │  later, see STTEngine.h): "take a photo"
        ▼
CommandManager::dispatchGlassesCommand("take a photo")
        │
        ▼
CameraManager::capture() → camera_fb_t
        │
        ▼
SDManager::writeFile("/OpenVisionEye/photos/IMG_xxxxxx.jpg", fb->buf, fb->len)
        │
        ▼
WiFiManager::sendControl("PHOTO_TAKEN")   AND   CommandManager::say("Photo taken.")
        │                                              │
        ▼                                              ▼
   (Audio hub logs it)                    ESP32 Audio logs the "SAY:" line —
                                           see "no TTS yet" note below; it does
                                           not produce audio in v1.1.1
```

Single-click / double-click on the same button skip the listening window
entirely and fire PHOTO / VIDEO_TOGGLE directly.

## Data flow — "Jarvis, glasses, take a photo" (SECONDARY/legacy, disabled by default)

```
BOOT button press (only if wakeWordEnabled=true) or Serial/TCP "JARVIS"
        │
        ▼
WakeWordEngine::poll() → WakeWord::JARVIS
        │
        ▼
CommandManager: Stage::MODE_SELECT (6s window)
        │  next text line: "glasses"
        ▼
CommandManager::onModeWord() → Stage::GLASSES_COMMAND (8s window)
        │  next text line: "take a photo"
        ▼
(same as the primary flow from here)
```

This flow still works exactly as before — it's real, tested code — it's
just no longer what `docs/installation.md` walks you through first, and its
automatic BOOT-button trigger is off by default (see `ConfigManager.h`).
`HEY_AI` still works as a bench shortcut straight into the AI-question
window.

### Honest note on "take a photo" as spoken English (superseded by Part 1, v1.2)

Full command **speech-to-text** (turning arbitrary spoken English into a
string like "take a photo") is a different, harder problem than wake-word
spotting. As of v1.2, `CommandManager` can accept a recognized command via
four paths: (a) Serial Monitor text — bench testing, (b) the TCP control
port, (c) the button's single/double/long-click shortcuts, and (d)
`MultiNetSTT`'s command IDs when `VOICE_MODE=MULTINET` and
`OVE_ENABLE_MULTINET` are both set — see "Part 1" above for exactly how
verified that path is (short version: real code, not proven to boot on
this board yet).

### Honest note on `SAY:` and text-to-speech

`CommandManager::say()` sends `SAY:<text>` to the ESP32 Audio hub over the
control channel. In both v1.0 and v1.1, the Audio hub **only logs this
string over its own Serial** — see `ESP32_Audio.ino`'s `handleControlLine()`.
No audio is produced. This was true before this revision and remains true;
it is called out again here because it's easy to misread the diagram above
as implying spoken feedback exists. Wiring `SAY:` to actual playback (a
small canned-prompt WAV table played through the same `AudioOutput` path
music already uses) is a small, well-contained follow-up — not done in
v1.1 because it wasn't part of this revision's scope.
