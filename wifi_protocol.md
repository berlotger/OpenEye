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
| `VOLUME:<0-100>` | either | Set output volume |
| `BATTERY` | either | Request battery status from the other board |
| `BATTERY:<0-100 or UNKNOWN>` | either | Battery status reply |
| `PHOTO_TAKEN` | XIAO → Audio | A photo was just captured (Audio may play a shutter/confirmation sound) |
| `VIDEO_STARTED` | XIAO → Audio | Video recording started |
| `VIDEO_STOPPED` | XIAO → Audio | Video recording stopped |
| `AUDIO_START` | XIAO → Audio | A PCM stream is about to begin on the audio channel |
| `AUDIO_END` | XIAO → Audio | The PCM stream on the audio channel has ended |
| `PING` | either | Liveness check |
| `PONG` | either | Reply to `PING` |

Unknown commands are logged and ignored — the protocol is meant to be extended
(new verbs) without breaking older firmware on the other board, per the
"extensible" requirement.

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
