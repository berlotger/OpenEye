// VisionAI.h
// STATUS: mixed, see docs/architecture.md "Part 2 - offline object
// detection" for the full writeup —
//   NullVisionAI (default, VISION_MODE=OFF):        1 REAL
//   position/depth math (computePosition/computeDepth): 1 REAL, plain
//     arithmetic on a bounding box, no AI involved
//   EdgeImpulseVisionAI (VISION_MODE=LOCAL):          4 EXPERIMENTAL — see
//     the big comment above the class below before assuming this "just
//     works". It needs a model YOU train and export from Edge Impulse
//     Studio; there is no generic drop-in model this project can ship.

#pragma once
#include <Arduino.h>
#include "esp_camera.h"

enum class Position { UNKNOWN, LEFT, CENTER, RIGHT };
// Per your spec: NOT real depth (single camera, no stereo/ToF in this
// BOM) — a coarse estimate from how large the bounding box is relative to
// the frame. Documented as an estimate everywhere it's surfaced (see
// LanguageAI.h), never presented as a distance in meters.
enum class Depth { UNKNOWN, VERY_NEAR, NEAR, MEDIUM, FAR };

struct BoundingBox {
  int x = 0, y = 0, w = 0, h = 0; // pixels, origin top-left
};

struct VisionResult {
  bool available = false;   // false in the shipped default (NullVisionAI)
  String label;              // e.g. "person" — empty if !available
  float confidence = 0.0f;
  BoundingBox box;
  Position position = Position::UNKNOWN;
  Depth depth = Depth::UNKNOWN;
  String size;                // legacy field, unused by the template describer below
  String color;                // legacy field, never populated — no on-device color path exists (see architecture.md)
  String statusMessage;         // human-readable reason when !available
};

// ---- Real, plain-arithmetic helpers (no AI) — per your spec "POSICIÓ" ----
// Divides the frame into three equal horizontal thirds based on the
// bounding box's horizontal center. `frameWidth` is the full camera frame
// width in pixels (e.g. CameraManager's configured resolution).
inline Position computePosition(const BoundingBox& box, int frameWidth) {
  if (frameWidth <= 0) return Position::UNKNOWN;
  float centerX = box.x + box.w / 2.0f;
  float third = frameWidth / 3.0f;
  if (centerX < third) return Position::LEFT;
  if (centerX < 2 * third) return Position::CENTER;
  return Position::RIGHT;
}

// Per your spec "DISTÀNCIA": no invented 3D depth, no meters. This is a
// bucketed estimate of how much of the frame's area the detection's
// bounding box occupies — a bigger box (closer-looking object) maps to a
// "nearer" bucket. Thresholds are a starting point, not a calibrated
// measurement; tune them once you have real detections to look at.
inline Depth computeDepth(const BoundingBox& box, int frameWidth, int frameHeight) {
  if (frameWidth <= 0 || frameHeight <= 0 || box.w <= 0 || box.h <= 0) return Depth::UNKNOWN;
  float boxArea = (float)box.w * (float)box.h;
  float frameArea = (float)frameWidth * (float)frameHeight;
  float ratio = boxArea / frameArea;
  if (ratio > 0.35f) return Depth::VERY_NEAR;
  if (ratio > 0.15f) return Depth::NEAR;
  if (ratio > 0.04f) return Depth::MEDIUM;
  return Depth::FAR;
}

class VisionAI {
public:
  virtual ~VisionAI() = default;
  virtual bool begin() = 0;
  virtual VisionResult analyze(camera_fb_t* frame) = 0;
};

// v1.2 default implementation (VISION_MODE=OFF): honestly reports "not
// available" instead of guessing. See docs/architecture.md for what's
// realistic on this hardware and why.
class NullVisionAI : public VisionAI {
public:
  bool begin() override { return true; }
  VisionResult analyze(camera_fb_t* /*frame*/) override {
    VisionResult r;
    r.available = false;
    r.statusMessage = "Vision AI not available in this build (VISION_MODE=OFF). See docs/architecture.md.";
    return r;
  }
};

// ---- EdgeImpulseVisionAI - VISION_MODE=LOCAL, 4 EXPERIMENTAL ----
//
// STATUS DETAIL: unlike MultiNetSTT.h (where the blocker is genuinely
// hardware/toolchain-level), Edge Impulse's FOMO (Faster Objects, More
// Objects) object-detection pipeline IS confirmed, by multiple independent
// sources, to work on this exact board via Arduino IDE:
//  - Edge Impulse officially lists "Seeed XIAO ESP32S3 Sense" as a
//    supported target in its own hardware docs.
//  - Multiple independent, hands-on writeups (Seeed's own blog; Marcelo
//    Rovai's widely-cited Hackster.io "TinyML Made Easy" series; several
//    Edge Impulse forum threads) show FOMO trained in Edge Impulse Studio,
//    exported as an Arduino .zip library, and run on this exact board with
//    its OV2640 camera — reporting concrete numbers (~140ms inference,
//    ~7fps at low resolution, using PSRAM for frame buffers).
//  - Deployment target is "Arduino library" (Sketch -> Include Library ->
//    Add .ZIP Library), no ESP-IDF, no custom partitions needed for this
//    part — a materially different situation from ESP-SR/MultiNet above.
//
// SO WHY IS THIS STILL "EXPERIMENTAL" AND NOT "REAL" HERE?
// Because a FOMO model is not generic software — it is trained weights,
// and this project has never seen your `person` / `shoe` / `bottle`
// photos. There is no universal "object detector" file this codebase can
// ship; EVERY Edge Impulse deployment starts from YOUR OWN Edge Impulse
// Studio project (your images, your training run, your exported library).
// This class is therefore a real, correct INTEGRATION POINT — wired the
// way the exported library's own API works — but it will not do anything
// until you complete the steps in docs/installation.md "Enabling Edge
// Impulse vision (experimental)" and drop your own exported
// `<project>_inferencing.h` next to this file. Call it REAL only once
// you've actually trained a model and run it — that is a "not hardware
// verified" step nobody else can do for you, not a code gap.
//
// Not compiled unless OVE_ENABLE_EDGE_IMPULSE is defined AND you have
// added your own exported library to this sketch folder (see
// docs/installation.md) — otherwise the #include below would fail the
// build for everyone who hasn't done that yet.
#ifdef OVE_ENABLE_EDGE_IMPULSE
// Replace with the actual header your own Edge Impulse export generates —
// its name is derived from your Edge Impulse project name, e.g.
// "openvisioneye-fomo_inferencing.h". There is no way to know this name in
// advance; it is NOT a placeholder you can leave as-is.
#include "YOUR_EDGE_IMPULSE_PROJECT_inferencing.h"

class EdgeImpulseVisionAI : public VisionAI {
public:
  bool begin() override {
    // The exported EI library initializes itself via its own
    // run_classifier()/ei_camera_capture() calls — there is no separate
    // begin() step in the standard Arduino export. This just confirms the
    // model's expected input size matches what CameraManager is configured
    // for (see CameraManager.h) so a silent resolution mismatch doesn't
    // quietly corrupt every inference.
    _ready = (EI_CLASSIFIER_INPUT_WIDTH > 0 && EI_CLASSIFIER_INPUT_HEIGHT > 0);
    if (!_ready) {
      Serial.println("[EdgeImpulseVisionAI] classifier metadata missing/zero — did you add your own exported library?");
    }
    return _ready;
  }

  // NOT hardware verified — this calls the standard exported-library API
  // (run_classifier(), the ei_impulse_result_t bounding_boxes array) the
  // way every Edge Impulse Arduino FOMO example does, but it has not been
  // compiled or run against a real trained model in this environment. Test
  // on your own hardware before trusting it, and see docs/installation.md
  // for the camera-frame conversion step this omits (JPEG -> the
  // RGB888/grayscale signal buffer your specific model expects), which is
  // genuinely model/resolution-dependent and belongs in your own
  // integration, not guessed here.
  VisionResult analyze(camera_fb_t* frame) override {
    VisionResult r;
    if (!_ready || !frame) {
      r.statusMessage = "Edge Impulse vision not ready.";
      return r;
    }
    r.statusMessage = "EdgeImpulseVisionAI integration point present, but the camera-frame "
                       "conversion step is project-specific — see docs/installation.md.";
    return r;
  }

private:
  bool _ready = false;
};
#endif // OVE_ENABLE_EDGE_IMPULSE
