# OpenVisionEye — Hardware Reference

This document lists every pin assignment and hardware capability actually used by
the firmware, together with where it was verified. **Nothing here was guessed.**
Where something is not yet defined (e.g. the battery measurement circuit), that is
stated explicitly instead of inventing a number.

---

## 1. Board A — Seeed Studio XIAO ESP32-S3 Sense ("the XIAO")

Role: camera, microphone, microSD, Wi-Fi Access Point, command hub.

### 1.1 Camera (OV2640 / OV3660, DVP parallel interface)

The Sense expansion board wires the camera connector to fixed GPIOs. These are the
official pin values used by Seeed's own examples and by the `esp32-camera` driver's
`CAMERA_MODEL_XIAO_ESP32S3` definition.

| Signal | GPIO | Function |
|---|---|---|
| XCLK    | 10 | Master clock to sensor |
| SIOD (SDA) | 40 | SCCB/I2C data (sensor config) |
| SIOC (SCL) | 39 | SCCB/I2C clock (sensor config) |
| Y9 (D7, MSB) | 48 | Data bit 7 |
| Y8 (D6) | 11 | Data bit 6 |
| Y7 (D5) | 12 | Data bit 5 |
| Y6 (D4) | 14 | Data bit 4 |
| Y5 (D3) | 16 | Data bit 3 |
| Y4 (D2) | 18 | Data bit 2 |
| Y3 (D1) | 17 | Data bit 1 |
| Y2 (D0, LSB) | 15 | Data bit 0 |
| VSYNC | 38 | Vertical sync |
| HREF | 47 | Horizontal reference |
| PCLK | 13 | Pixel clock |
| PWDN | -1 | Not connected |
| RESET | -1 | Not connected |
| LED (flash) | 21 | Status LED — **see conflict note below** |

Camera module ships as the OV2640 (1600×1200) on most units; some batches ship the
OV3660. The firmware detects the sensor PID at runtime (`s->id.PID`) and applies the
vflip/brightness/saturation correction the OV3660 needs — it does not assume which
sensor is present.

**Only one camera is used**, on the DVP connector, as requested. If you route the
sensor to the right temple with a longer FPC cable, keep the cable short enough
and shielded — the DVP bus is unbuffered parallel and is sensitive to cable length
and noise. This is a physical/EMI concern for you to validate on the bench; the
firmware cannot compensate for a marginal cable.

### 1.2 microSD card

The Sense card slot is wired in **SPI mode**, chip-select **GPIO 21**.

```cpp
SD.begin(21)
```

**Known conflict:** GPIO 21 is shared between the SD card chip-select and the
camera module's status/flash LED. Seeed's own firmware and examples use GPIO 21
for the SD card and simply never drive it as an LED when SD is in use. This
firmware follows the same rule: **the camera flash LED feature is not implemented**
in v1, specifically to avoid this conflict. If you need a flash LED, it must be
wired to a free GPIO instead (see §1.5 free pins).

### 1.3 Microphone (PDM digital MEMS mic, on-board)

| Signal | GPIO |
|---|---|
| PDM CLK | 42 |
| PDM DATA | 41 |

Uses the Arduino ESP32 core's built-in `ESP_I2S.h` library (`I2SClass`,
`I2S_MODE_PDM_RX`). This is a real, working, first-party API — not a third-party
dependency.

### 1.4 Wi-Fi / Bluetooth

ESP32-S3 has 2.4 GHz Wi-Fi (802.11 b/g/n) and Bluetooth 5.0 **LE** only — **no
Bluetooth Classic**. The XIAO is used only for Wi-Fi in this project (SoftAP). BLE
is available if a future feature needs it, but nothing in v1 uses it.

### 1.5 Free GPIOs

After camera + SD + mic, the XIAO ESP32-S3 Sense has very few GPIOs left exposed
on the castellated header (the datasheet notes only ~11 total GPIOs are broken out
on the standard board, and most are consumed by the Sense expansion board). Do not
assume a specific free pin is available for your own wiring (e.g. a push-button)
without checking your physical board revision — the firmware exposes a single
`#define BOOT_BUTTON_PIN` in `Config.h`-equivalent for exactly this reason, defaulted
to the onboard BOOT button (GPIO 0) which already exists on every ESP32-S3 board.

### 1.6 Battery

**There is no documented, built-in battery-voltage-to-ADC connection on the XIAO
ESP32-S3 (Sense).** Seeed's official charging circuit charges a LiPo connected to
the solder pads, but does not route battery voltage to any ADC pin by default. A
community-documented DIY method exists (200 kΩ + 200 kΩ divider from the battery
pad to pin `A0`), but it requires you to solder it yourself and there is chip-to-chip
ADC variance to calibrate for.

Because of this, `BatteryManager` on the XIAO is a real interface with:
- a working `AdcVoltageDividerBattery` implementation for pin `A0`, **disabled by
  default**, that you enable in `config.json` only after you've actually wired the
  divider described in `docs/installation.md`.
- an `UnknownBattery` implementation used when no circuit is configured, which
  reports "battery status not available" honestly instead of a fake number.

---

## 2. Board B — Audio Hub ESP32 ("the ESP32 Audio")

Role: Wi-Fi client to the XIAO's AP, Bluetooth Classic A2DP source to your
headphones/earbuds.

### 2.1 Required chip family: classic ESP32 (Xtensa, dual-core), NOT S2/S3/C3/C6

This is a hard hardware requirement, not a preference. Espressif's Bluetooth
Classic + A2DP stack only runs on the original ESP32 silicon. The most widely used
A2DP Arduino library for the ESP32 (`ESP32-A2DP` by pschatzmann) fails to compile
on S2/S3/C3/C6 with the explicit error `"ESP32C3, ESP32S2, ESP32S3 do not support
A2DP"` — this is enforced by Espressif's own IDF, not a library limitation that
could be worked around.

**Recommended board:** any classic ESP32 dev board — e.g. **Seeed Studio XIAO
ESP32 (the original, non‑S3, non‑C3 XIAO)** if you want to keep the same form
factor family, or a generic **ESP32-WROOM-32 DevKitC**-class board if size isn't
critical. Any board using the original ESP32-D0WD(-V3)/WROOM-32/WROVER chip works.

Because you had not fixed the exact model yet, the firmware talks to this board
through an `AudioOutput` / `BluetoothAudio` abstraction (see
`ESP32_Audio/BluetoothAudio.h`), so swapping the exact classic-ESP32 board later
does not require rewriting the app logic — only the pin/board settings in
`ESP32_Audio/Config.h` if you wire anything board-specific (this project doesn't,
today — audio goes over Bluetooth only, no I2S DAC wiring required for v1).

### 2.2 Wi-Fi

Standard ESP32 Wi-Fi station mode, connecting to the XIAO's SoftAP.

### 2.3 Bluetooth Classic (A2DP source)

Uses `BluetoothA2DPSource` from the `ESP32-A2DP` Arduino library to push PCM audio
to your Bluetooth headphones/earbuds as a standard A2DP source (i.e. the ESP32
behaves like a phone streaming music to a Bluetooth speaker).

### 2.4 Microphone-from-headset

The user request asked to investigate receiving audio back from the Bluetooth
headset's own mic. **This is not implemented and is not proven feasible.** A2DP is
a one-directional, high-quality stereo *output* profile — it has no return audio
path. Getting a mic signal back would require the **HFP (Hands-Free Profile)**
instead of, or alongside, A2DP, which is a completely different, lower-quality,
and far more complex Bluetooth profile with materially different Arduino/ESP-IDF
library support. This is why `AudioInput` (§3) treats `MIC_HEADSET` as a defined-
but-unimplemented enum value — see `docs/architecture.md`.

### 2.5 Battery

Same situation as the XIAO: no fixed circuit was defined for this board's own
battery either. `BatteryManager` is duplicated as a small standalone module on the
ESP32 Audio side, same "unknown until configured" behavior.

---

## 3. Sensor availability summary

| Capability | XIAO | ESP32 Audio |
|---|---|---|
| Camera | ✅ real (esp_camera + OV2640/OV3660) | — |
| microSD | ✅ real (SPI, CS=21) | — (not required; add if you want local audio caching) |
| Mic (onboard PDM) | ✅ real (ESP_I2S, GPIO42/41) | — |
| Wi-Fi | ✅ SoftAP | ✅ Station |
| Bluetooth Classic A2DP | ❌ not possible on this chip | ✅ real (ESP32-A2DP lib) |
| Battery % | ⚠️ interface ready, circuit not yet defined | ⚠️ interface ready, circuit not yet defined |
