// ButtonManager.cpp
// STATUS: 1 REAL (see header)

#include "ButtonManager.h"

ButtonAction buttonActionFromString(const String& sIn) {
  String s = sIn; s.toUpperCase(); s.trim();
  if (s == "PHOTO") return ButtonAction::PHOTO;
  if (s == "VIDEO_TOGGLE") return ButtonAction::VIDEO_TOGGLE;
  if (s == "COMMAND_MODE") return ButtonAction::COMMAND_MODE;
  return ButtonAction::NONE;
}

String buttonActionToString(ButtonAction a) {
  switch (a) {
    case ButtonAction::PHOTO: return "PHOTO";
    case ButtonAction::VIDEO_TOGGLE: return "VIDEO_TOGGLE";
    case ButtonAction::COMMAND_MODE: return "COMMAND_MODE";
    default: return "NONE";
  }
}

void ButtonManager::begin() {
  pinMode(_pin, INPUT_PULLUP);
  _lastState = digitalRead(_pin);
  Serial.printf("[ButtonManager] ready on GPIO%d (single=%s double=%s long=%s)\n",
    _pin, buttonActionToString(_singleAction).c_str(),
    buttonActionToString(_doubleAction).c_str(),
    buttonActionToString(_longAction).c_str());
}

ButtonClick ButtonManager::poll() {
  bool state = digitalRead(_pin); // LOW while pressed (INPUT_PULLUP, active-low)

  // Falling edge: press started.
  if (_lastState == HIGH && state == LOW) {
    _pressedAt = millis();
  }

  // Rising edge: press released — classify it.
  ButtonClick result = ButtonClick::NONE;
  if (_lastState == LOW && state == HIGH) {
    unsigned long heldFor = millis() - _pressedAt;
    if (heldFor >= DEBOUNCE_MS) {
      if (heldFor >= LONG_PRESS_MS) {
        // Long press always fires immediately and cancels any pending
        // double-click wait — a long press is never mistaken for a click.
        _awaitingSecondClick = false;
        result = ButtonClick::LONG_PRESS;
      } else if (_awaitingSecondClick && (millis() - _firstClickAt) <= DOUBLE_CLICK_GAP_MS) {
        _awaitingSecondClick = false;
        result = ButtonClick::DOUBLE;
      } else {
        _awaitingSecondClick = true;
        _firstClickAt = millis();
        // Don't return SINGLE yet — wait out the double-click gap below in
        // case a second click follows. result stays NONE this call.
      }
    }
    // heldFor < DEBOUNCE_MS: contact bounce, ignored entirely.
  }
  _lastState = state;

  // Second chance: if we're waiting for a possible second click and the
  // gap has now expired with no new press, resolve it as SINGLE. This is
  // what makes single-click detection non-blocking — it doesn't happen on
  // the same poll() call as the release, but on a later one, without ever
  // calling delay().
  if (result == ButtonClick::NONE && _awaitingSecondClick &&
      (millis() - _firstClickAt) > DOUBLE_CLICK_GAP_MS) {
    _awaitingSecondClick = false;
    result = ButtonClick::SINGLE;
  }

  return result;
}

ButtonAction ButtonManager::actionFor(ButtonClick click) const {
  switch (click) {
    case ButtonClick::SINGLE: return _singleAction;
    case ButtonClick::DOUBLE: return _doubleAction;
    case ButtonClick::LONG_PRESS: return _longAction;
    default: return ButtonAction::NONE;
  }
}
