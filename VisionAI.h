// VisionAI.h
// STATUS: 5 FUTURE — interface only. See docs/architecture.md
// "Vision AI reality check" for exactly what is and isn't realistic on this
// hardware, and why. Do not wire this up to fake/random output.

#pragma once
#include <Arduino.h>
#include "esp_camera.h"

enum class Position { UNKNOWN, LEFT, CENTER, RIGHT };
enum class Depth    { UNKNOWN, NEAR, MID, FAR };

struct VisionResult {
  bool available = false;   // false in the shipped v1 implementation, always
  String label;              // e.g. "person" — empty if !available
  float confidence = 0.0f;
  Position position = Position::UNKNOWN;
  Depth depth = Depth::UNKNOWN;
  String size;                // e.g. "medium" — empty if !available
  String color;                // e.g. "dark" — empty if !available
  String statusMessage;         // human-readable reason when !available
};

class VisionAI {
public:
  virtual ~VisionAI() = default;
  virtual bool begin() = 0;
  virtual VisionResult analyze(camera_fb_t* frame) = 0;
};

// v1 shipped implementation: honestly reports "not available" instead of
// guessing. See docs/architecture.md for the recommended real next step
// (on-device presence/face detection via esp-dl, or offload the JPEG to a
// phone/server).
class NullVisionAI : public VisionAI {
public:
  bool begin() override { return true; }
  VisionResult analyze(camera_fb_t* /*frame*/) override {
    VisionResult r;
    r.available = false;
    r.statusMessage = "Vision AI not available in this build. See docs/architecture.md.";
    return r;
  }
};
