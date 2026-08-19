# OpenVisionEye — Troubleshooting

## XIAO diagnostics show `[FAIL] Camera`

- Most common cause: PSRAM not enabled. **Tools → PSRAM → OPI PSRAM**, then
  re-upload.
- Check the FPC cable orientation — the blue/contacts side convention
  differs by cable batch; if it was working before you lengthened the
  cable for the temple mount (see `docs/hardware.md` §1.1), reseat it and
  verify continuity before assuming it's a firmware issue.
- Try Seeed's own bare camera example first (outside this project) to
  isolate hardware vs. firmware — a link is in their wiki tutorial
  referenced in `docs/hardware.md`.

## XIAO diagnostics show `[FAIL] SD`

- Confirm the card is formatted **FAT32** (not exFAT — that's a common trap
  with cards larger than 32GB, which default to exFAT when reformatted by
  some OSes).
- Reseat the card — the Sense board's slot is a friction-fit push type.
- Try a different, known-good card.
- Remember GPIO21 is shared with the (unused, in this project) flash LED —
  see `docs/hardware.md` §1.2. If you've modified the firmware to also
  drive an LED on GPIO21, that's your SD failure.

## XIAO diagnostics show `[FAIL] Wi-Fi AP`

- Rare — usually indicates a bad `esp32` core install. Try reinstalling the
  board package via Boards Manager.
- Make sure no other device/script forced Wi-Fi into a bad state — power
  cycle the board.

## ESP32 Audio board never finds the XIAO's SoftAP

- Confirm the XIAO printed a `SSID: OpenVisionEye-XXXXXX` line in its own
  Serial Monitor — if the XIAO's Wi-Fi AP itself failed, fix that first.
- Check `ESP32_Audio/Config.h`'s `AP_SSID_PREFIX` matches the XIAO's
  `ConfigManager.h` `apSsidPrefix` (default `"OpenVisionEye"` on both).
- Distance: SoftAP range is modest, especially through a breadboard/enclosure
  — test with both boards a few centimeters apart first.
- `WiFiAudio::begin()` has a bounded timeout (`connectTimeoutMs`, default
  20 seconds) — if scanning is slow, it may need more than one attempt;
  the ESP32 Audio's `setup()` only tries once, so a failed first attempt
  means you'll need to reset it after the XIAO is confirmed up.

## ESP32 Audio board joins Wi-Fi but the control/audio TCP sockets don't connect

- Confirm `AP_PASSWORD` in `ESP32_Audio/Config.h` matches the XIAO's
  `apPassword` (default `"glasses1234"` on both) — a Wi-Fi association can
  succeed with a wrong password rejected differently depending on
  IDF version; check for `WL_CONNECTED` vs. an auth failure in the logs.
- Confirm `XIAO_AP_IP` (`192.168.4.1` by default) hasn't been changed on
  the XIAO side — it isn't, in this project's `WiFiManager`, but if you've
  customized it, update both.

## Compiles but Bluetooth pairing never happens

- Double-check you selected a **classic ESP32** board, not an S3 lookalike
  — `docs/hardware.md` §2.1 has the exact symptom (a compile-time error
  from the `ESP32-A2DP` library) you'd see if you picked the wrong chip, so
  if it compiled at all, the chip choice is probably fine and this is a
  pairing/discovery issue instead.
- Set `HEADSET_BT_NAME` in `Config.h` to the exact name your headphones
  show when pairing with a phone — leaving it blank makes connection
  behavior much less predictable (see `docs/installation.md` §9).
- Put the headphones into pairing mode fresh (many auto-exit pairing mode
  after ~1-2 minutes) and reset the ESP32 Audio board so it retries from a
  clean state.
- Some headphones are picky about Secure Simple Pairing — check the
  `ESP32-A2DP` library's own GitHub issues/discussions for your specific
  headphone model; this is a known source of device-specific quirks that
  this project's own code can't work around.

## Audio plays but sounds choppy or glitchy

- This is expected under sustained Wi-Fi congestion or very small SD reads
  — `AudioOutput`'s ring buffer (`~1 second` by default) will glitch
  audibly rather than crash if it underruns; see `AudioOutput.h`.
- The nearest-sample upsampling from 16kHz to 44.1kHz (see
  `docs/architecture.md`) is intentionally simple, not high-fidelity — some
  roughness on music is expected in v1.
- Try moving the boards closer together / reducing 2.4GHz interference from
  other Wi-Fi networks in the room.

## "Battery status is not available"

This is the correct, expected response until you wire the DIY divider
circuit described in `docs/hardware.md` §1.6 and enable it in
`config.json` — see `docs/installation.md` §11. It is not a bug.

## Commands aren't recognized

- Remember v1 takes command **text**, not spoken English — see
  `docs/architecture.md` "Honest note on 'Hey Glasses, take a photo'". Type
  the command into Serial Monitor after triggering a wake word.
- The listening window is 8 seconds (`CommandManager::LISTEN_WINDOW_MS`) —
  if you waited longer than that after pressing BOOT, trigger it again.
- Check `dispatchGlassesCommand()` / `dispatchAiCommand()` in
  `CommandManager.cpp` for the exact phrase matching used, and extend the
  `containsIgnoreCase(...)` checks there for your own phrasing if needed.
