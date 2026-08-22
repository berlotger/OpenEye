// ButtonManager.h
// STATUS: 1 REAL — plain digitalRead() polling with a non-blocking
// millis()-based state machine. No blocking delay() anywhere (per your
// requirement that button handling must not stall the rest of the
// firmware).
//
// This is a SEPARATE physical button from the one WakeWordEngine uses.
// WakeWordEngine's manual-trigger stub uses the onboard BOOT button
// (GPIO0) to inject the "Jarvis" wake word. This ButtonManager is for a
// NEW push-button you wire yourself, dedicated to fast photo/video/
// command-mode shortcuts that don't need to wait for the Jarvis flow.
//
// ---- GPIO choice: why GPIO2 (silkscreen "D1") ----
// The XIAO ESP32-S3 Sense has almost no free GPIOs once the camera (10-18,
// 38,39,40,47,48), mic (41,42), SD card CS (21), and BOOT button (0) are
// accounted for — see docs/hardware.md §1.5, which explicitly warns not to
// pick a pin blindly. The pins actually free on the castellated header are
// the ones Seeed documents as the general-purpose expansion header:
// D0=GPIO1, D1=GPIO2, D2=GPIO3, D3=GPIO4, D4=GPIO5(I2C SDA), D5=GPIO6(I2C
// SCL), D6=GPIO43(TX), D7=GPIO44(RX), D8=GPIO7(SPI SCK), D9=GPIO8(SPI
// MISO), D10=GPIO9(SPI MOSI). None of these overlap the camera/mic/SD/BOOT
// pins above.
//
// Of those, D2/GPIO3, D6/GPIO43, and D7/GPIO44 carry boot-strapping or
// USB-serial duties and are best avoided for a simple input. GPIO2 (D1)
// carries none of those roles and is a commonly recommended "safe first
// pick" pin on this exact board. It is documented here, in
// docs/hardware.md, and cross-referenced against every other pin already
// in use in this project (see the comment block above) — not picked
// blindly.
//
// STILL VERIFY ON YOUR PHYSICAL BOARD: pin silkscreen/label placement has
// varied slightly across XIAO ESP32-S3 (Sense) production batches. Confirm
// "D1" on your board with a multimeter/continuity check against this GPIO
// number before soldering, exactly as docs/hardware.md §1.5 already warns
// for any custom wiring on this board.

#pragma once
#include <Arduino.h>

enum class ButtonAction {
  NONE,
  PHOTO,
  VIDEO_TOGGLE,
  COMMAND_MODE,
};

enum class ButtonClick {
  NONE,
  SINGLE,
  DOUBLE,
  LONG_PRESS,
};

// Converts to/from the strings stored in config.json ("PHOTO",
// "VIDEO_TOGGLE", "COMMAND_MODE", "NONE") and used in the BUTTON_SINGLE: /
// BUTTON_DOUBLE: / BUTTON_LONG: runtime remap commands (see
// CommandManager.cpp).
ButtonAction buttonActionFromString(const String& s);
String buttonActionToString(ButtonAction a);

class ButtonManager {
public:
  static constexpr int DEFAULT_BUTTON_GPIO = 2; // GPIO2 / silkscreen "D1" — see header note above

  explicit ButtonManager(int pin = DEFAULT_BUTTON_GPIO) : _pin(pin) {}

  void begin();

  // Non-blocking. Call every loop() iteration. Returns the click type
  // detected this call, or ButtonClick::NONE most calls. A DOUBLE click is
  // only reported after the double-click gap window elapses without a
  // third press (so a SINGLE click is never falsely reported first).
  ButtonClick poll();

  // Runtime-configurable mapping (loaded from / saved to config.json by
  // ConfigManager; see CommandManager's BUTTON_SINGLE:/BUTTON_DOUBLE:/
  // BUTTON_LONG: text commands for changing this without reflashing).
  void setMapping(ButtonAction single, ButtonAction dbl, ButtonAction longPress) {
    _singleAction = single; _doubleAction = dbl; _longAction = longPress;
  }
  ButtonAction singleAction() const { return _singleAction; }
  ButtonAction doubleAction() const { return _doubleAction; }
  ButtonAction longAction()   const { return _longAction; }

  // Convenience: resolve a ButtonClick straight to the currently-mapped
  // ButtonAction (ButtonAction::NONE if unmapped).
  ButtonAction actionFor(ButtonClick click) const;

private:
  int _pin;
  bool _lastState = true; // active-low with INPUT_PULLUP: HIGH = not pressed

  // Debounce + click-timing state
  static constexpr unsigned long DEBOUNCE_MS = 30;
  static constexpr unsigned long LONG_PRESS_MS = 800;   // matches WakeWordEngine's own threshold, for a consistent feel
  static constexpr unsigned long DOUBLE_CLICK_GAP_MS = 350;

  unsigned long _pressedAt = 0;
  bool _awaitingSecondClick = false;
  unsigned long _firstClickAt = 0;

  ButtonAction _singleAction = ButtonAction::PHOTO;
  ButtonAction _doubleAction = ButtonAction::VIDEO_TOGGLE;
  ButtonAction _longAction   = ButtonAction::COMMAND_MODE;
};
