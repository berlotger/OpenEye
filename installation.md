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
   -------------------------
   SSID: OpenVisionEye-XXXXXX
   AP IP: 192.168.4.1
   Ready. Press BOOT briefly for "Hey Glasses", hold >0.8s for "Hey AI".
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

## 10. First end-to-end test

With both boards powered and connected:

1. On the XIAO, briefly press the **BOOT** button (this is the manual
   "Hey Glasses" wake-word stub — see `docs/architecture.md`).
2. Within 8 seconds, type a command into the XIAO's Serial Monitor and
   press Enter, e.g.:
   ```
   take a photo
   ```
3. You should see, on the XIAO:
   ```
   [SAY] Photo taken.
   ```
   and a new file under `/OpenVisionEye/photos/` on the SD card (remove
   the card and check on a computer, or add your own SD-listing command if
   you want one over Serial — not included by default to keep the firmware
   small).
4. Try:
   ```
   battery status
   ```
   You should get `Battery status is not available. See docs/hardware.md.`
   — this is the **expected, honest** response until you wire the optional
   battery-divider circuit described there.
5. If you've copied a WAV file into `/OpenVisionEye/music/`, try:
   ```
   play music
   ```
   and you should hear it (quietly, and with basic-quality resampling —
   see `docs/architecture.md`) through your Bluetooth headphones.

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
