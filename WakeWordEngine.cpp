// WakeWordEngine.cpp
// STATUS: 4 EXPERIMENTAL (see header)

#include "WakeWordEngine.h"

bool ManualTriggerWakeWord::begin() {
  pinMode(_buttonPin, INPUT_PULLUP);
  _lastState = digitalRead(_buttonPin);
  Serial.println("[WakeWordEngine] manual trigger ready (BOOT button + Serial/TCP inject)");
  return true;
}

WakeWord ManualTriggerWakeWord::poll() {
  if (_injected != WakeWord::NONE) {
    WakeWord w = _injected;
    _injected = WakeWord::NONE;
    return w;
  }

  bool state = digitalRead(_buttonPin); // LOW while pressed
  if (_lastState == HIGH && state == LOW) {
    _pressedAt = millis();
  }
  if (_lastState == LOW && state == HIGH) {
    unsigned long heldFor = millis() - _pressedAt;
    _lastState = state;
    if (heldFor > 800) {
      return WakeWord::HEY_AI;
    } else if (heldFor > 30) { // debounce
      return WakeWord::HEY_GLASSES;
    }
  }
  _lastState = state;
  return WakeWord::NONE;
}
