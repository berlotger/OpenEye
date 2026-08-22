// STTEngine.h
// STATUS: 5 FUTURE (interface only) — NOT wired into CommandManager.
//
// Read this whole comment before using this file — it explains an
// honesty tradeoff, not just an API.
//
// In v1.1, CommandManager gets command text directly from Serial and from
// the TCP control channel (see WiFiManager::setControlLineHandler /
// CommandManager::handleIncomingText). That IS, functionally, what
// "manual/mock STT" already means: a human types the words a real STT
// engine would have decoded. Adding a `ManualSTT` class that just wraps
// that same Serial/TCP text and calling it "wired in" would be cosmetic —
// it would not change any real behavior, and could misleadingly look like
// more STT integration exists than actually does.
//
// So: this file defines the STTEngine interface your spec asked for, so a
// real implementation (LocalSTT, OnlineSTT, PhoneSTT) has somewhere to
// plug in later without CommandManager's dispatch logic changing. It also
// ships MockSTT, a trivial test double for exercising STTEngine-based code
// in isolation (e.g. a future unit test harness) — clearly marked TEST,
// and NOT referenced anywhere in the main .ino or CommandManager. Wiring a
// real engine in means: implement this interface, then change
// CommandManager to call poll() instead of reading Serial/TCP text
// directly — a small, contained change when that day comes.
//
// No real local, online, or phone-relayed STT exists in this codebase.

#pragma once
#include <Arduino.h>

class STTEngine {
public:
  virtual ~STTEngine() = default;
  virtual bool begin() = 0;
  // Non-blocking. Returns true and fills `outText` if a phrase finished
  // decoding since the last call; returns false most calls.
  virtual bool poll(String& outText) = 0;
};

// ---- TEST ONLY — not used anywhere in the shipped firmware ----
// Lets you feed one canned phrase to STTEngine-shaped test code without a
// real engine. Call inject() from a test harness, then poll() once to
// receive it.
class MockSTT : public STTEngine {
public:
  bool begin() override { return true; }
  bool poll(String& outText) override {
    if (!_hasPending) return false;
    outText = _pending;
    _hasPending = false;
    return true;
  }
  void inject(const String& text) { _pending = text; _hasPending = true; }

private:
  String _pending;
  bool _hasPending = false;
};
