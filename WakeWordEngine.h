// WakeWordEngine.h
// STATUS: 4 EXPERIMENTAL — interface is real/stable, the shipped
// implementation is an explicit manual-trigger stub, NOT keyword spotting on
// audio. See docs/architecture.md "Why wake word is a stub" for the reasoning
// and the ESP-SR / Picovoice Porcupine pointers for a real replacement.

#pragma once
#include <Arduino.h>

enum class WakeWord {
  NONE = 0,
  HEY_GLASSES,   // system commands
  HEY_AI,        // local AI (VisionAI/LanguageAI pipeline)
  HEY_JARVIS,    // reserved for a future advanced assistant, v1 no-op
};

class WakeWordEngine {
public:
  virtual ~WakeWordEngine() = default;
  virtual bool begin() = 0;
  // Non-blocking. Returns WakeWord::NONE most calls.
  virtual WakeWord poll() = 0;
};

// ---- Shipped v1 implementation: manual trigger, not audio-based ----
// - Short press of the BOOT button (GPIO0)  -> HEY_GLASSES
// - Long press (>800ms) of the BOOT button  -> HEY_AI
// - Serial or TCP control text "HEY_GLASSES" / "HEY_AI" / "HEY_JARVIS" also
//   injects a wake word — see ManualTriggerWakeWord::inject().
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
  unsigned long _pressedAt = 0;
  WakeWord _injected = WakeWord::NONE;
};
