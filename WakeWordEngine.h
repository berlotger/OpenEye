// WakeWordEngine.h
// STATUS: 4 EXPERIMENTAL — interface is real/stable, the shipped
// implementation is an explicit manual-trigger stub, NOT keyword spotting
// on audio. See docs/architecture.md "Why wake word is a stub" for the
// full reasoning, including specifically why "Jarvis" is not something
// this project can honestly claim as a built-in/native ESP32 feature (it
// is not — see that section before assuming otherwise).
//
// v1.1 change: the wake word is now a single word, "Jarvis", per your
// request. What comes after it (a *mode* word like "glasses"/"ai"/"music")
// is now CommandManager's job — see CommandManager.h's two-stage
// MODE_SELECT flow. This keeps WakeWordEngine itself simple and gives you
// room to add new modes later without touching this file.

#pragma once
#include <Arduino.h>

enum class WakeWord {
  NONE = 0,
  JARVIS,
};

class WakeWordEngine {
public:
  virtual ~WakeWordEngine() = default;
  virtual bool begin() = 0;
  // Non-blocking. Returns WakeWord::NONE most calls.
  virtual WakeWord poll() = 0;
};

// ---- Shipped v1.1 implementation: manual trigger, not audio-based ----
// - Any press/release of the BOOT button (GPIO0), regardless of duration,
//   injects WakeWord::JARVIS. (Duration-based shortcuts — short vs. long
//   press — now live on the separate, optional ButtonManager button, which
//   is a different physical button for direct photo/video/command-mode
//   shortcuts. See ButtonManager.h. Conflating the two on one pin was a
//   source of confusion in v1.0 and is deliberately split here.)
// - Serial or TCP control text "JARVIS" / "HEY_JARVIS" also injects the
//   wake word — see ManualTriggerWakeWord::inject().
class ManualTriggerWakeWord : public WakeWordEngine {
public:
  explicit ManualTriggerWakeWord(int buttonPin) : _buttonPin(buttonPin) {}
  bool begin() override;
  WakeWord poll() override;

  // Lets CommandManager forward a text-injected wake word (from Serial/TCP)
  // into the same queue the button uses.
  void inject(WakeWord w) { _injected = w; }

private:
  int _buttonPin;
  bool _lastState = true;   // BOOT button is active-low with internal pullup
  static constexpr unsigned long DEBOUNCE_MS = 30;
  unsigned long _pressedAt = 0;
  WakeWord _injected = WakeWord::NONE;
};
