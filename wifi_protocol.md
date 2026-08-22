# OpenVisionEye — Wi-Fi Protocol between XIAO and ESP32 Audio

## Topology

```
XIAO ESP32-S3 Sense                    ESP32 Audio (classic ESP32)
   Wi-Fi SoftAP                            Wi-Fi Station
   SSID:  OpenVisionEye-XXXX                connects to the above
   IP:    192.168.4.1 (default ESP SoftAP)  gets 192.168.4.2 (or similar) via DHCP
   |
   |-- TCP :3333  Control channel (text line protocol)
   |-- TCP :3334  Audio channel   (binary, length-prefixed PCM)
```

The XIAO runs both TCP servers. The ESP32 Audio is the client for both, and
reconnects automatically if the link drops (see `WiFiAudio::loop()`).

There is **no internet access, router, or phone involved** — this is a private,
local, XIAO-hosted network, matching the offline requirement.

## Control channel (TCP port 3333)

ASCII text, one command per line, terminated by `\n`. Case-sensitive, upper-case
verbs. This is intentionally simple (easy to log, easy to debug with `nc` or a
serial-to-TCP bridge) rather than a binary protocol, since control messages are
tiny and infrequent.

| Command | Direction | Meaning |
|---|---|---|
| `SAY:<text>` | XIAO → Audio | Speak `<text>` — Audio should play the corresponding local TTS/prompt if available, otherwise ignore |
| `PLAY` | either | Resume/start music playback |
| `PAUSE` | either | Pause music playback |
| `STOP` | either | Stop playback entirely |
| `NEXT` | either | Skip to next track |
| `PREVIOUS` | either | Go to previous track |
| `VOLUME:<0-100>` | either | Set output volume — v1.1.1: the XIAO now actually sends this (`CommandManager::doSetVolume()`, triggered by the "volume \<n\>" command); the Audio hub has understood it since v1.0 (`bluetoothAudio.setVolume()`) |
| `BATTERY` | either | Request battery status from the other board |
| `BATTERY:<0-100 or UNKNOWN>` | either | Battery status reply |
| `PHOTO_TAKEN` | XIAO → Audio | A photo was just captured (Audio may play a shutter/confirmation sound) |
| `VIDEO_STARTED` | XIAO → Audio | Video recording started |
| `VIDEO_STOPPED` | XIAO → Audio | Video recording stopped |
| `AUDIO_START` | XIAO → Audio | A PCM stream is about to begin on the audio channel |
| `AUDIO_END` | XIAO → Audio | The PCM stream on the audio channel has ended |
| `PING` | either | Liveness check |
| `PONG` | either | Reply to `PING` |

Unknown Audio-hub protocol tokens don't just get logged and dropped: the XIAO
falls them through to `CommandManager::handleIncomingText()`, the same path
Serial input uses. That's what lets a PC on the XIAO's SoftAP — e.g.
`tools/send_command.py` — drive the button-equivalent command pipeline over
TCP (`HEY_GLASSES`, then e.g. `take a photo`) without an on-device STT
engine, and also what makes the diagnostic and button-config commands below
work from either Serial or TCP:

| Command | Direction | Meaning |
|---|---|---|
| `AUDIO_TEST` | → XIAO | Generates a 440Hz/1s sine tone and streams it down the real pipeline (Wi-Fi → ESP32 Audio → A2DP → headphones), through whichever `AudioBackend` is active. If you hear it, the whole audio path works, independent of the mic or SD card. See `AudioManager::generateSineTone()`. |
| `PING_TEST` | → XIAO | XIAO sends `PING` to the Audio hub and reports the round-trip time in ms once `PONG` comes back (logged over Serial). Different from raw `PING`/`PONG`, which is just a liveness check with no timing. |
| `HEY_GLASSES` | → XIAO | **Primary test shortcut in v1.1.1**: opens the same single-stage command-listening window a button long-press would — bench-testing stand-in for the physical button, see `docs/architecture.md`. Next line is a command, e.g. `take a photo`, `record audio`, `volume 50`. |
| `JARVIS` / `HEY_JARVIS` | → XIAO | Legacy/secondary path (disabled-by-default automatic trigger, see `wakeWordEnabled` in `config.json`): injects the "Jarvis" wake word manually — opens a mode-select window. This text command always works regardless of the flag. See `docs/architecture.md`. |
| `HEY_AI` | → XIAO | Skips straight to the AI-question listening window (part of the legacy Jarvis-flow's two modes) without needing `JARVIS` + a mode word first. |
| `BUTTON_SINGLE:<ACTION>` | → XIAO | Remaps `ButtonManager`'s single-click action and persists it to `config.json`. `<ACTION>` is one of `PHOTO`, `VIDEO_TOGGLE`, `COMMAND_MODE`, `NONE`. |
| `BUTTON_DOUBLE:<ACTION>` | → XIAO | Same, for double-click. |
| `BUTTON_LONG:<ACTION>` | → XIAO | Same, for long-press. |

Once inside the command-listening window (via the button or `HEY_GLASSES`),
the recognized command phrases are: `take a photo`, `start recording`,
`stop recording`, `record audio` (v1.1.1 — fixed 5s WAV to
`/OpenVisionEye/audio/`), `volume <0-100>` (v1.1.1), `play music`, `pause
music`, `next song`, `previous song`, `battery` — see
`CommandManager::dispatchGlassesCommand()` for the exact matching.

Note: the `BUTTON_*` and `JARVIS`/`HEY_*` commands are handled entirely on
the XIAO — they are local firmware config/test commands, not part of the
XIAO↔Audio-hub protocol proper. They're listed here because they happen to
travel over the same TCP control port when sent from a PC tool (see
"single client only" below), not because the ESP32 Audio board does
anything with them.

**Single client only (v1 limitation):** the control server (port 3333) and
audio server (port 3334) each hold exactly one TCP client at a time. In
normal operation that's the ESP32 Audio board on both. If you connect a PC
test tool to port 3333 while the real Audio hub is also trying to connect,
they contend for that one control-channel slot — fine for bench testing,
not something to rely on together in the field. A future version could add
a second, PC-only diagnostic port instead of sharing this one; not done in
v1 to avoid extra surface area before the core pipeline is proven.

## Audio channel (TCP port 3334)

Text is not sent here — this is for **streaming PCM**, per the requirement to not
push audio over a text protocol.

Format: raw signed 16-bit little-endian PCM, mono, 16000 Hz (matches the XIAO's
onboard PDM mic's practical sample rate and keeps the ESP32 Audio's decode side
trivial). Framed as:

```
[uint32_t frame_length_bytes, little-endian][frame_length_bytes of PCM data]
...repeated until AUDIO_END arrives on the control channel...
```

A length prefix (rather than a fixed chunk size) lets either side use whatever
buffer size is convenient without the receiver needing to guess boundaries.

`AUDIO_START` / `AUDIO_END` on the control channel bracket a streaming session so
the receiver knows when to open/close its output sink (A2DP write loop) without
needing an explicit "stream length" up front (recordings and live mic capture
don't have a known length in advance).

## Reliability

TCP already gives in-order, lossless delivery on the local link, which is enough
for this project's scale (two boards, one local AP, sub-10-meter range). No
custom retry/ack layer is implemented in v1. `PING`/`PONG` exists so either side
can detect a stale connection and reconnect — see `CommandManager::loop()` and
`WiFiAudio::loop()`.

## Why not ESP-NOW?

ESP-NOW was considered but not used: it's connectionless/best-effort, has a small
payload limit per packet (250 bytes), and offers no ordering guarantees across
packets — all of which make it a poor fit for streaming audio, and it adds
complexity for the plain command channel over TCP-on-SoftAP without a clear
benefit here. If a future version needs lower latency than TCP/SoftAP allows,
re-evaluate ESP-NOW for the control channel only (not audio).
