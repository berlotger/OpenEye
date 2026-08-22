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

- Remember this version takes command **text**, not spoken English — see
  `docs/architecture.md` "Honest note on 'take a photo' as spoken English".
  Long-press the physical button (or type `HEY_GLASSES`) to open the
  listening window, then type your command into the Serial Monitor within
  8 seconds (`CommandManager::LISTEN_WINDOW_MS`) — there's no way to
  resume a closed window; trigger it again from the start.
- Check `dispatchGlassesCommand()` / `dispatchAiCommand()` in
  `CommandManager.cpp` for the exact phrase matching used, and extend the
  `containsIgnoreCase(...)` checks there for your own phrasing if needed.
- Using the legacy Jarvis path (`wakeWordEnabled: true`)? That flow adds a
  *second*, separate 6-second mode-select window
  (`CommandManager::MODE_SELECT_WINDOW_MS`) before the command window — see
  `docs/installation.md` §10b.

## Typing "JARVIS" does nothing / no "listening for a mode" message

- The `JARVIS`/`HEY_JARVIS` text shortcuts work regardless of
  `wakeWordEnabled`; confirm you typed exactly that (case-insensitive, but
  no typos) — anything else falls through to the normal command path and
  will just log "got text but nothing is listening" if no window is open.
- The **BOOT-button** auto-trigger for Jarvis only fires if
  `wakeWordEnabled: true` is set in `config.json` (it's `false` by default
  this version — see `docs/architecture.md`). The physical `ButtonManager`
  button's long-press is the default trigger instead, and doesn't depend
  on this flag at all.
- If you're testing over TCP (`tools/send_command.py`) instead of Serial,
  remember the control port only holds one client — if the real ESP32
  Audio hub is also connected, your test client may not be the one that
  got through. See `docs/wifi_protocol.md` "Single client only".

## The physical button (ButtonManager) does nothing

- It's a **separate physical button from BOOT** — see
  `docs/hardware.md` §1.5 and `docs/installation.md` §10. Pressing BOOT
  only ever affects the legacy Jarvis stub (and only if `wakeWordEnabled`
  is enabled); it does not run `ButtonManager` at all.
- Confirm you wired it to GPIO2 ("D1") to GND, not another pin — and that
  you've actually verified "D1" against your specific board's silkscreen,
  since `docs/hardware.md` §1.5 notes labeling has varied slightly across
  production batches.
- A single click is only reported *after* the double-click gap (350ms)
  elapses with no second press — so there's a short, expected delay
  between releasing the button and seeing the `[Button] click=... ->
  action=PHOTO` line in Serial. That delay is normal, not a bug.
- Check the current mapping with `DIAG`, or re-send it with
  `BUTTON_SINGLE:PHOTO` / `BUTTON_DOUBLE:VIDEO_TOGGLE` /
  `BUTTON_LONG:COMMAND_MODE` — see `docs/wifi_protocol.md`.

## "record audio" seems to freeze everything for a few seconds

That's expected — see `README.md` "Known limitations". `doRecordAudio()`
blocks the main loop for a fixed 5 seconds (camera capture, Wi-Fi control
polling, and button polling all pause). It is not a crash or a hang if it
recovers on its own after ~5s and prints `Audio saved.` (or a failure
message if the SD card wasn't ready).

## "volume 50" doesn't seem to change the loudness

- Confirm the Audio hub is actually connected (`DIAG` on the XIAO should
  show `[OK] Audio hub link`) — `VOLUME:` is sent over the control TCP
  socket, so it silently does nothing if that socket isn't up yet.
- Check the **Audio hub's** own Serial Monitor for a line from
  `BluetoothAudio::setVolume()` — if you don't see one, the command didn't
  arrive; re-check the control channel.
- `BluetoothA2DPSource::set_volume()`'s actual audible effect depends on
  your specific headphones' own volume handling — some Bluetooth headsets
  ignore the source-side volume and only respond to their own physical
  buttons, which this project has no way to detect or override.

## `AUDIO_TEST` or music plays through the wrong path, or `PHONE` warnings appear in the log

- `PhoneAudioBackend` is **not implemented** — see `AudioBackend.h`. If
  `audioBackendMode` in `config.json` is `"PHONE"`, you'll see a one-time
  `[AudioBackendManager] AUDIO_MODE=PHONE but PhoneAudioBackend is not
  implemented ... using LOCAL instead` log line, then it plays through
  LOCAL exactly as before. This is expected, not a failure — no phone
  audio path exists to select yet.
- To confirm which backend is actually in use, type `DIAG` — it logs
  `Audio backend: LOCAL (mode=...)`.
- If you want to force LOCAL explicitly (e.g. while testing), set
  `"audioBackendMode": "LOCAL"` in `config.json`, or send the mode with a
  future `AUDIO_MODE:` control command once you add one (not implemented
  as a runtime text command in v1.1 — only the field in `config.json` is
  read, at boot).

## `E MODEL_LOADER: Can not find model in partition table` / crash at boot with `OVE_ENABLE_MULTINET` defined

This is the exact, known failure mode documented in
`docs/architecture.md` "Part 1 — offline voice command recognition" — a
dated (March 2026) forum report shows this exact crash on this exact board
family (`XIAO_ESP32S3(_PLUS)`) running Espressif's own official example.
It means the current **Tools > Partition Scheme** doesn't reserve a
MultiNet model partition — it is not something in this project's own code
that can fix it. See `docs/installation.md` "Enabling MultiNet
(experimental)" step 2 for what to try. Test with
`examples/MultiNet_Test/MultiNet_Test.ino` in isolation before assuming
the problem is in the main firmware's integration.

## Edge Impulse: compile errors about `EI_CLASSIFIER_INPUT_WIDTH` undefined, or the model header not found

- You haven't added your own exported Edge Impulse Arduino library yet
  (Sketch > Include Library > Add .ZIP Library), and/or you haven't
  replaced the placeholder `#include "YOUR_EDGE_IMPULSE_PROJECT_inferencing.h"`
  line in `VisionAI.h` / `examples/Vision_Test/Vision_Test.ino` with your
  own header's real filename. See `docs/installation.md` "Enabling Edge
  Impulse vision (experimental)". There is no generic model this project
  ships — every deployment starts from your own trained project.

## Edge Impulse: detections look wrong, or every frame reports the same box

- Almost always a camera-frame-to-model-input mismatch — check that the
  resolution/format you're feeding `run_classifier()` actually matches
  `EI_CLASSIFIER_INPUT_WIDTH`/`HEIGHT` and the color format (FOMO models
  are commonly trained on grayscale). Copy the conversion code from your
  exported library's own bundled `esp32 > esp32_camera` example rather
  than the commented-out sketch of it in `examples/Vision_Test.ino` — that
  example is written and tested against exactly this class of mismatch by
  Edge Impulse themselves.

## `VOICE_MODE:MULTINET` or `VISION_MODE:LOCAL` accepted over Serial, but nothing changes

- Both are **build-time gated** (`OVE_ENABLE_MULTINET` /
  `OVE_ENABLE_EDGE_IMPULSE` at the top of `XIAO_OpenVisionEye.ino`) as well
  as **runtime-gated** (the `config.json` field). Setting the runtime mode
  without the matching `#define` + recompile does nothing except persist
  the setting for later — this is intentional (see `ConfigManager.h`),
  not a bug. `VOICE_MODE` also only takes effect after a reboot, since
  `MultiNetSTT` is only constructed/`begin()`'d once at startup.
