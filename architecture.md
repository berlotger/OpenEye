# OpenVisionEye — Software Architecture

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
| `WakeWordEngine` | 4 EXPERIMENTAL | interface + a manual-trigger stub (button/Serial/HTTP), see below |
| `CommandManager` | 1 REAL | dispatches text commands from Serial / stub wake-word / TCP to the other modules |
| `VisionAI` | 5 FUTURE | interface only — see "Vision AI reality check" below |
| `LanguageAI` | 5 FUTURE | interface only — see below |
| `ConfigManager` | 1 REAL | JSON config file on SD via ArduinoJson |

## Module map — ESP32 Audio (patilla esquerra / left temple)

| Module | Status | Notes |
|---|---|---|
| `WiFiAudio` | 1 REAL | Wi-Fi station, connects to XIAO's control+audio TCP servers |
| `BluetoothAudio` | 2 REAL (needs lib) | `ESP32-A2DP` library, `BluetoothA2DPSource` |
| `AudioOutput` | 1 REAL | buffers PCM received over Wi-Fi and feeds it to the A2DP callback |
| `BatteryManager` | 3 NEEDS HARDWARE | same situation as XIAO's |

## Why wake word is a stub, not fake keyword spotting

You explicitly asked not to fake this with a naive string search, so here is the
honest state of the art as of this project:

- A **real, on-device wake-word engine** for ESP32-S3 exists: Espressif's own
  **ESP-SR** (WakeNet for wake word, MultiNet for a small fixed command
  vocabulary). It is real and does run on the S3, but it is distributed as an
  ESP-IDF component with only partial/community Arduino wrappers, needs a model
  blob matched to your exact wake phrase, and is a substantial integration effort
  on its own — too large and too fragile to respectably fake into this delivery
  without you being able to test it incrementally.
- `WakeWordEngine` is therefore a clean interface (`begin()`, `poll()` returning
  which wake word if any) with one implementation shipped:
  `ManualTriggerWakeWord`, which is triggered by the **BOOT button** (short press
  = "Hey Glasses", long press = "Hey AI") and by sending the literal strings
  `HEY_GLASSES` / `HEY_AI` over Serial or the diagnostic TCP port. This lets you
  exercise the entire rest of the pipeline (commands → camera/audio/Wi-Fi) today.
- "Hey Jarvis" is defined as a constant and routed through the same interface,
  but has **no v1 behavior** — it's reserved for the "future advanced AI" hook per
  your spec and currently just logs "Jarvis is not implemented yet."
- To add real wake-word detection later: implement `WakeWordEngine` with ESP-SR
  (or a hosted alternative like Picovoice Porcupine, which does have first-party
  ESP32 support) and swap it in `CommandManager::begin()`. No other file needs to
  change.

## Vision AI reality check

You asked for structured output like:

```
OBJECT: person   CONFIDENCE: 0.91   POSITION: right   DEPTH: far   SIZE: medium   COLOR: dark
```

Honest breakdown of what's actually achievable on an ESP32-S3 today:

- **Object presence/class detection** (person vs. not) at low resolution is
  realistic on-device with Espressif's own `esp-dl` face-detection models or a
  small quantized model via Edge Impulse — this is the only part of "Vision AI"
  that has a credible on-device path today, and even then it's closer to "is
  there a face/person-shaped blob" than general multi-class object detection.
- **Multi-class detection** (person / animal / vehicle / etc., simultaneously,
  in real time) is not realistically achievable on this chip's compute/RAM
  budget with acceptable accuracy — this is not a library gap, it's a hardware
  ceiling.
- **Depth ("far/near"), color naming, and "young-looking" style attributes** from
  a single 2D camera have no reliable on-device solution here at all — real depth
  needs stereo or a depth sensor, which isn't in your BOM.

Because of this, `VisionAI` is shipped as an **interface only**
(`VisionAI::analyze(camera_fb_t*) -> VisionResult`), with a single
`NullVisionAI` implementation that returns "vision AI not available in this
build" instead of inventing numbers. It is structured so you can plug in:
(a) a small on-device presence detector for "is there a person" as a first real
step, or (b) send the JPEG frame over Wi-Fi to a phone/server/API for full
analysis later — the interface doesn't care which, it just returns a
`VisionResult` struct.

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

## Data flow — "Hey Glasses, take a photo"

```
BOOT button short-press (stub wake word)
        │
        ▼
WakeWordEngine::poll() → WAKE_GLASSES
        │
        ▼
CommandManager reads next command text ("take a photo" from Serial/TCP, or a
button-mapped shortcut in v1 since ASR isn't implemented — see below)
        │
        ▼
CameraManager::takePhoto() → camera_fb_t
        │
        ▼
SDManager::writeFile("/OpenVisionEye/photos/IMG_xxx.jpg", fb->buf, fb->len)
        │
        ▼
WiFiManager::sendControl("PHOTO_TAKEN")   AND   WiFiManager::sendControl("SAY:Photo taken.")
        │                                              │
        ▼                                              ▼
   (Audio hub logs it)                    ESP32 Audio plays the "photo_taken"
                                           canned prompt from its own small
                                           prompt table (see AudioManager notes)
```

### Honest note on "Hey Glasses, take a photo" as spoken English

Full command **speech-to-text** (turning arbitrary spoken English into the string
"take a photo") is a different, harder problem than wake-word spotting and is
**not implemented** in v1 for the same hardware-budget reasons as `VisionAI`.
`CommandManager` in v1 accepts commands via: (a) Serial Monitor text — useful for
bench testing, (b) the TCP control port — useful once you build a companion
tool, and (c) button-mapped shortcuts. The wake-word stub + command dispatcher
plumbing is real and complete; only the "convert speech to the command string"
step is a gap, clearly reserved for ESP-SR's MultiNet (fixed small vocabulary,
which is enough for a command set like this) as the v1.1 candidate — see
`README.md` roadmap.
