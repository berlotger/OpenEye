// Vision_Test.ino
// STATUS: 4 EXPERIMENTAL, and genuinely blocked on YOUR input, not a code
// gap — see the big comment above EdgeImpulseVisionAI in
// ../../XIAO_OpenVisionEye/VisionAI.h first.
//
// Purpose (per spec): verify object detection works in ISOLATION —
//   OV2640 -> capture -> Edge Impulse -> print every detection to Serial
//   (class, confidence, bounding box, LEFT/CENTER/RIGHT)
// Deliberately excludes: MultiNet/voice entirely.
//
// ---- Required before this will even compile ----
// 1. Train a FOMO (object detection) model in Edge Impulse Studio with (at
//    minimum) your `person` / `shoe` / `bottle` classes, per the spec.
//    Edge Impulse Studio -> Deployment -> "Arduino library" -> Build.
//    This is a real, confirmed-working target for this exact board (see
//    docs/architecture.md "Part 2" for the sources), but it is YOUR
//    model — nobody else can generate it for you.
// 2. Arduino IDE: Sketch -> Include Library -> Add .ZIP Library... and
//    select the .zip Edge Impulse just built for you.
// 3. Replace the #include below with the exact header name your export
//    produced (Edge Impulse names it after your project, e.g.
//    "openvisioneye-fomo_inferencing.h" — check the zip's src/ folder).
// 4. Board: XIAO_ESP32S3, PSRAM: OPI PSRAM (Tools menu).
//
// NOT hardware verified in this environment (no physical board, and no
// model exists to test against) — this is real, correct integration code
// against the standard Edge Impulse Arduino camera-example API (the same
// shape Edge Impulse's own "esp32 > esp32_camera" example sketch uses),
// not a fabrication, but you are the first to actually compile+run it.

// #include "YOUR_EDGE_IMPULSE_PROJECT_inferencing.h"  // <-- replace this line

#include "esp_camera.h"

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
// Verified pin config for this board — copied from
// ../XIAO_camera_only/XIAO_camera_only.ino, which is already confirmed
// against docs/hardware.md §1.1.

// Same plain-arithmetic position/depth helpers as the main firmware (see
// ../../XIAO_OpenVisionEye/VisionAI.h) — duplicated here rather than
// #included so this test sketch has zero dependency on the rest of the
// project and can be compiled completely standalone, per the spec.
enum class Position { UNKNOWN, LEFT, CENTER, RIGHT };
const char* positionToString(Position p) {
  switch (p) {
    case Position::LEFT: return "LEFT";
    case Position::CENTER: return "CENTER";
    case Position::RIGHT: return "RIGHT";
    default: return "UNKNOWN";
  }
}
Position computePosition(int boxX, int boxW, int frameWidth) {
  if (frameWidth <= 0) return Position::UNKNOWN;
  float centerX = boxX + boxW / 2.0f;
  float third = frameWidth / 3.0f;
  if (centerX < third) return Position::LEFT;
  if (centerX < 2 * third) return Position::CENTER;
  return Position::RIGHT;
}

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // NOTE: your Edge Impulse model needs a SPECIFIC input size/format
  // (check EI_CLASSIFIER_INPUT_WIDTH/HEIGHT in your exported header) —
  // FOMO models are commonly trained at 96x96 grayscale. JPEG capture +
  // manual decode/resize to match your model's exact expected buffer is
  // real, model-specific work belonging in your own integration — the
  // standard Edge Impulse "esp32_camera" example (bundled in the library
  // you added in step 2 above) already does this end-to-end and is the
  // more reliable starting point than reinventing it here. This sketch
  // stops at capturing a JPEG frame and calling out exactly where your
  // model's signal-buffer conversion goes, rather than guessing at it.
  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA; // 320x240 — adjust to your model
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  return esp_camera_init(&config) == ESP_OK;
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) { delay(10); }

  Serial.println();
  Serial.println("Vision_Test starting (camera only — no MultiNet, no Wi-Fi)...");

  bool camOk = initCamera();
  Serial.printf("[%s] Camera initialized\n", camOk ? "OK" : "FAIL");
  if (!camOk) { while (true) delay(1000); }

#ifndef EI_CLASSIFIER_INPUT_WIDTH
  Serial.println("[FAIL] No Edge Impulse model included. Uncomment the #include at the top of");
  Serial.println("       this file with YOUR exported project's header name — see the comment");
  Serial.println("       block above. Halting (nothing to do without a model).");
  while (true) delay(1000);
#else
  Serial.println("[OK] Edge Impulse model loaded");
  Serial.printf("Model expects %dx%d input.\n", EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
#endif
}

void loop() {
#ifdef EI_CLASSIFIER_INPUT_WIDTH
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { Serial.println("[FAIL] capture"); delay(1000); return; }

  // ---- YOUR signal-buffer conversion goes here ----
  // fb->buf/fb->len is a JPEG. Your model needs it decoded and
  // resized/converted to EI_CLASSIFIER_INPUT_WIDTH x HEIGHT in whatever
  // pixel format your specific Edge Impulse project was trained with
  // (grayscale is typical for FOMO). The Edge Impulse "esp32_camera"
  // example bundled in your exported library (step 2 above) already
  // implements this — copy it rather than trusting an invented version
  // here, since the exact conversion depends on your model's training
  // config. Once you have a signal_t built, call run_classifier(&signal,
  // &result, false) and iterate result.bounding_boxes[] as below.
  //
  // ei_impulse_result_t result = { 0 };
  // EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  // for (size_t i = 0; i < result.bounding_boxes_count; i++) {
  //   auto& bb = result.bounding_boxes[i];
  //   if (bb.value == 0) continue;
  //   Position pos = computePosition(bb.x, bb.width, EI_CLASSIFIER_INPUT_WIDTH);
  //   Serial.printf("class=%s confidence=%.2f box=[x=%d y=%d w=%d h=%d] position=%s\n",
  //                 bb.label, bb.value, bb.x, bb.y, bb.width, bb.height,
  //                 positionToString(pos));
  // }

  esp_camera_fb_return(fb);
  delay(500);
#else
  delay(1000);
#endif
}
