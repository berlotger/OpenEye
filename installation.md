# OpenVisionEye — Installation & First Test

Follow this in order. Both boards need to be flashed; the "first test" at the
end only works once both are.

## 1. Install Arduino IDE

Download and install Arduino IDE 2.x from https://www.arduino.cc/en/software.

## 2. Add the ESP32 board package

1. Open **File → Preferences** (Arduino IDE → Settings on macOS).
2. In "Additional boards manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search **esp32**, install the
   package by **Espressif Systems** (latest 3.x).

## 3. Select the board for each sketch

- For `XIAO_OpenVisionEye.ino`: **Tools → Board → esp32 → XIAO_ESP32S3**
- For `ESP32_Audio.ino`: **Tools → Board → esp32 → ESP32 Dev Module** (or
  your specific classic-ESP32 board entry — must NOT be an S2/S3/C3/C6
  variant, see `docs/hardware.md` §2.1)

## 4. XIAO-specific board settings

With `XIAO_OpenVisionEye.ino` open and the XIAO_ESP32S3 board selected:

- **Tools → PSRAM → OPI PSRAM** (required — see `requirements.md`)
- **Tools → USB CDC On Boot → Enabled** (so Serial Monitor works over the
  native USB port without a manual reset dance)
- **Tools → Partition Scheme → Default 4MB with spiffs (or similar default)**

## 5. Install libraries

**Tools → Manage Libraries...**, then install:

- `ArduinoJson` by Benoit Blanchon (latest 7.x)
- `ESP32-A2DP` by Phil Schatzmann (latest)

See `requirements.md` for exactly what each is used for and what's already
built into the core (don't install those separately).

## 6. Prepare the microSD card (XIAO)

- Format as FAT32.
- You don't need to pre-create the `/OpenVisionEye/...` folders — the
  firmware creates them on first boot (`SDManager::ensureDirectoryStructure`).
- Optionally copy some mono 16-bit 16kHz WAV files into `/OpenVisionEye/music/`
  now (see `requirements.md` for the `ffmpeg` conversion command) so you have
  something to test `PLAY`/`NEXT` with later.
- Insert the card into the Sense board's slot.

## 7. Flash the XIAO

1. Open `XIAO_OpenVisionEye/XIAO_OpenVisionEye.ino`.
2. Connect the XIAO via USB-C.
3. **Tools → Port**, select the XIAO's port.
4. Click **Upload**.
5. Open **Tools → Serial Monitor**, set baud to **115200**.
6. You should see:
   ```
   OpenVisionEye starting...
   ---- DIAGNOSTIC MODE ----
   [OK] Camera
   [OK] SD
   [OK] Wi-Fi AP
   [OK] Microphone
   [N/A] Battery (no circuit configured, see docs/hardware.md)
   [--] Audio ESP32 — will report once it connects over Wi-Fi
   [SKIP] Button (GPIOx) — wiring not verified in this environment, see docs/hardware.md
   [--] Audio backend mode: AUTO
   -------------------------
   SSID: OpenVisionEye-XXXXXX
   AP IP: 192.168.4.1
   Ready.
   Primary control: long-press the button (GPIO2/"D1") to enter command mode,
   then type a command — e.g. "take a photo", "record audio", "battery".
   Single-click = photo, double-click = video start/stop (see docs/hardware.md).
   No button wired yet? Type HEY_GLASSES on the Serial Monitor to open the same
   command-listening window, then type your command.
   (Optional legacy path: the "Jarvis" wake-word stub is disabled by default in this version — see config.json's
    wakeWordEnabled. JARVIS/HEY_JARVIS/HEY_AI text shortcuts still work either way.)
   ```
   If Camera/SD/Wi-Fi show FAIL, see `docs/troubleshooting.md` before
   continuing.

## 8. Flash the ESP32 Audio board

1. Open `ESP32_Audio/ESP32_Audio.ino`.
2. Before uploading, check `ESP32_Audio/Config.h`:
   - `AP_SSID_PREFIX` and `AP_PASSWORD` must match what's in the XIAO's
     `ConfigManager.h` defaults (`"OpenVisionEye"` / `"glasses1234"`) unless
     you changed `config.json` on the XIAO's SD card — if you did, update
     `Config.h` to match.
   - Set `HEADSET_BT_NAME` to your headphones' exact Bluetooth name if you
     want a deterministic connection (see step 9).
3. Connect the ESP32 Audio board via USB.
4. **Tools → Port**, select its port (a *different* port than the XIAO —
   keep both boards plugged in if you want to watch both Serial Monitors).
5. Click **Upload**.
6. Open Serial Monitor at 115200. You should see it scan for and join the
   XIAO's SoftAP, then connect both TCP ports:
   ```
   OpenVisionEye Audio Hub starting...
   [WiFiAudio] scanning for SoftAP starting with "OpenVisionEye"...
   [WiFiAudio] connecting to OpenVisionEye-XXXXXX
   [WiFiAudio] connected, IP 192.168.4.2
   [WiFiAudio] control channel connected
   [WiFiAudio] audio channel connected
   ---- DIAGNOSTIC MODE ----
   [OK] Wi-Fi link to XIAO
   [OK] Audio output buffer
   [OK] Bluetooth A2DP source started
   [--] Bluetooth headset link — connects once paired, see installation.md
   -------------------------
   Ready.
   ```
7. Back on the **XIAO's** Serial Monitor, you should now see the control
   client connect and the "Audio hub connected." announcement go out.

If you don't see this within ~20 seconds, see `docs/troubleshooting.md`.

## 9. Pair your Bluetooth headphones

`BluetoothA2DPSource::start()` behaves differently depending on whether you
gave it a name:

- **If you set `HEADSET_BT_NAME`** to your headphones' exact Bluetooth name
  (check this in your phone's Bluetooth settings — it's whatever name shows
  up when pairing), the ESP32 Audio board will actively try to find and
  connect to that specific device.
- **If you left it blank**, the library falls back to its own
  discovery/auto-reconnect behavior (it will try the last-connected device,
  or scan) — less predictable for a first test. Setting the exact name is
  recommended.

Put your headphones into pairing mode, reset the ESP32 Audio board, and
watch its Serial Monitor for connection state logs from the `ESP32-A2DP`
library (it logs quite verbosely by default, which is useful here).

## 10. First end-to-end test (button-driven — PRIMARY flow this version)

With both boards powered and connected, wire a normal momentary push-button
between **GPIO2 ("D1")** and **GND** — no external resistor needed, the
firmware enables the internal pull-up (`INPUT_PULLUP`). See `docs/hardware.md`
§1.5 for why this pin was picked and what else was checked against it.

Don't have the button wired yet? Every step below also works by typing
`HEY_GLASSES` into the XIAO's Serial Monitor instead of long-pressing the
button — it opens the identical command-listening window.

1. **Long-press** the button (or type `HEY_GLASSES`). You have 8 seconds to
   type a command into the Serial Monitor, e.g.:
   ```
   take a photo
   ```
2. You should see, on the XIAO:
   ```
   [SAY] Photo taken.
   ```
   and a new file under `/OpenVisionEye/photos/` on the SD card (remove
   the card and check on a computer, or add your own SD-listing command if
   you want one over Serial — not included by default to keep the firmware
   small).
3. Each command needs a fresh long-press (or `HEY_GLASSES`) — try:
   ```
   battery
   ```
   You should get `Battery status is not available. See docs/hardware.md.`
   — this is the **expected, honest** response until you wire the optional
   battery-divider circuit described in step 11.
4. Try the new v1.1.1 audio-recording command:
   ```
   record audio
   ```
   This blocks for a fixed 5 seconds (documented limitation — see
   `README.md`), then saves `/OpenVisionEye/audio/REC_000001.wav`.
5. Try the new v1.1.1 volume command:
   ```
   volume 40
   ```
   You should see `[SAY] Volume set to 40.` and, on the **ESP32 Audio
   board's** Serial Monitor, a log line from `BluetoothAudio::setVolume()`.
6. If you've copied a WAV file into `/OpenVisionEye/music/`, long-press (or
   `HEY_GLASSES`) again, then:
   ```
   play music
   ```
   and you should hear it (quietly, and with basic-quality resampling —
   see `docs/architecture.md`) through your Bluetooth headphones.
7. Outside the physical button, **single-click** should take a photo
   immediately (no listening window), and **double-click** should start,
   then later stop, video recording.
8. To change the button mapping without reflashing, type e.g.
   `BUTTON_SINGLE:VIDEO_TOGGLE` in the Serial Monitor — see
   `docs/wifi_protocol.md`. The change is saved to `config.json`
   immediately.

## 10b. (Optional/legacy) The "Jarvis" two-stage flow

This still works, but is **disabled by default** this version
(`wakeWordEnabled: false` in `config.json`) and is not the primary flow —
see `docs/architecture.md`. To try it: set `wakeWordEnabled: true` in
`/OpenVisionEye/config/config.json`, power-cycle, then:

1. Briefly press the **BOOT** button (or type `JARVIS` on the Serial
   Monitor). You have 6 seconds to respond.
2. Type a mode word and press Enter:
   ```
   glasses
   ```
   You now have 8 seconds to type a command, same as above.

The BOOT-button trigger only works with `wakeWordEnabled: true`; the
`JARVIS`/`HEY_JARVIS`/`HEY_AI` text shortcuts work regardless of the flag.

## 11. (Optional) Wire the battery-voltage divider

See `docs/hardware.md` §1.6. Once wired:

1. Edit `/OpenVisionEye/config/config.json` on the SD card (or delete it and
   let the firmware regenerate defaults, then edit):
   ```json
   "batteryAdcDividerEnabled": true,
   "batteryAdcPin": -1
   ```
2. Re-insert the card, power-cycle the XIAO.
3. `battery status` should now report a real percentage. Cross-check it
   against a multimeter on the battery pads at least once — the divider's
   exact resistor values and your specific ADC's calibration both affect
   accuracy, and this project doesn't attempt to auto-calibrate that for you.

## 12. (Optional, EXPERIMENTAL) Enabling MultiNet offline voice commands

Read `docs/architecture.md` "Part 1 — offline voice command recognition"
and `MultiNetSTT.h` first — this is not confirmed to work on the 8MB-flash
XIAO ESP32S3 Sense, and a dated forum report shows the stock example
crashing at boot on this exact board.

1. Test in isolation FIRST: open `examples/MultiNet_Test/MultiNet_Test.ino`
   on its own (not the main firmware). This has zero dependency on the
   rest of the project.
2. **Tools > Partition Scheme**: look for an entry that reserves a
   MultiNet model partition (something with "SR" in the name — Espressif's
   own examples use names like "ESP SR 16M"). If nothing like that appears
   for the `XIAO_ESP32S3` board, that is the actual blocker described in
   `docs/architecture.md` — there is no known workaround documented here,
   because none was found. Options to try, roughly in order of effort:
   - Check whether a newer `esp32` board package version (Boards Manager)
     added an 8MB-flash "esp_sr" partition entry for this board — this
     changes over time and should be re-checked.
   - Try `Tools > Custom Partition CSV` (if your Arduino IDE / board
     package version supports it) with a hand-written `partitions.csv`
     that reserves a `model` partition of a few MB, sized to fit in 8MB
     alongside your app — this requires understanding ESP32 partition
     tables; see Espressif's partition-table docs.
   - Investigate `CONFIG_MODEL_IN_SDCARD` (seen in `ESP_SR`'s own source)
     as an alternative to a flash model partition — NOT confirmed how (or
     whether) this is reachable from the Arduino IDE Tools menu for this
     board. If you get this working, please document it — this project
     genuinely doesn't know the answer.
3. **Tools > PSRAM**: OPI PSRAM (same as the main firmware).
4. Once `MultiNet_Test.ino` reliably prints `[OK] MultiNet model loaded`
   and recognizes real spoken commands, only then move to the main
   firmware:
   - Uncomment `#define OVE_ENABLE_MULTINET` at the top of
     `XIAO_OpenVisionEye.ino`.
   - Recompile and flash.
   - Send `VOICE_MODE:MULTINET` over Serial/TCP, then power-cycle (the
     mode is read once at boot).
   - Check `DIAG` output: `[OK] MultiNet model loaded` means it's live;
     `[FAIL]` means the same partition-table problem as step 2.
5. Regenerate the phonetic command table before trusting recognition
   accuracy — `MultiNetSTT.cpp`'s table was hand-written, not run through
   Espressif's own `tools/gen_sr_commands.py` in this environment (that
   script lives in the `esp-sr` repo/ESP-IDF component, not in this
   project). This is flagged in the source comment, not hidden.

## 13. (Optional, EXPERIMENTAL) Enabling Edge Impulse vision

Read `docs/architecture.md` "Part 2 — offline object detection" and the
big comment above `EdgeImpulseVisionAI` in `VisionAI.h` first. Unlike
MultiNet above, this path is confirmed by several independent sources to
work on this exact board — the remaining work is entirely on your side
(training a model), not a toolchain fight.

1. Create a free Edge Impulse account, create a new project.
2. Collect training images of `person` / `shoe` / `bottle` (per your
   spec's initial classes) — Edge Impulse's docs cover this; you can use
   the XIAO's own camera (via the existing `examples/XIAO_camera_only`
   sketch or a web-server capture sketch) or your phone.
3. In Edge Impulse Studio: add an "Object Detection" learning block, pick
   **FOMO (Faster Objects, More Objects)** as the architecture (not
   MobileNet-SSD — FOMO is the one confirmed practical on this chip's
   budget, per `docs/architecture.md`), train.
4. **Deployment tab > "Arduino library" > Build.** Download the `.zip`.
5. In Arduino IDE: **Sketch > Include Library > Add .ZIP Library...**,
   select the file you just downloaded.
6. Test in isolation FIRST: open `examples/Vision_Test/Vision_Test.ino`,
   uncomment its `#include` line and replace it with the exact header name
   your export produced (check the `.zip`'s `src/` folder — it's named
   after your Edge Impulse project). Under `File > Examples`, your
   library's own bundled `esp32 > esp32_camera` example is a more reliable
   starting point for the camera-frame-to-model-input conversion than the
   commented-out sketch of that step in `Vision_Test.ino` — copy the real
   conversion code from there.
7. Once detections print correctly to Serial with class/confidence/box/
   position, only then move to the main firmware: uncomment
   `#define OVE_ENABLE_EDGE_IMPULSE` at the top of `XIAO_OpenVisionEye.ino`,
   replace the placeholder `#include` in `VisionAI.h`'s
   `EdgeImpulseVisionAI` block with your real header name, send
   `VISION_MODE:LOCAL`, recompile, flash.
