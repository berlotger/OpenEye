// XIAO_camera_only.ino
// STATUS: 1 REAL — minimal isolation test.
//
// Purpose: verify the camera + SD hardware works BEFORE trying the full
// OpenVisionEye firmware, so a hardware problem (bad FPC cable, wrong SD
// card format) doesn't look like a software bug. See
// docs/troubleshooting.md "XIAO diagnostics show [FAIL] Camera/SD".
//
// Board: XIAO_ESP32S3, PSRAM: OPI PSRAM (Tools menu) — same settings as the
// main project, see docs/installation.md.
//
// Behavior: takes one photo every 10 seconds and saves it to the SD card
// root as test_<n>.jpg. Watch the Serial Monitor at 115200 baud.

#include "esp_camera.h"
#include <FS.h>
#include <SD.h>

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
#define SD_CS_PIN         21

int photoCount = 0;

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
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("XIAO camera-only test");
  Serial.printf("PSRAM found: %s\n", psramFound() ? "yes" : "no");

  bool camOk = initCamera();
  Serial.printf("Camera: %s\n", camOk ? "OK" : "FAIL");

  bool sdOk = SD.begin(SD_CS_PIN);
  Serial.printf("SD: %s\n", sdOk ? "OK" : "FAIL");

  if (!camOk || !sdOk) {
    Serial.println("Fix the failing component before testing the main project.");
  }
}

void loop() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("capture failed");
    delay(10000);
    return;
  }

  char path[32];
  snprintf(path, sizeof(path), "/test_%d.jpg", photoCount++);
  File f = SD.open(path, FILE_WRITE);
  if (f) {
    f.write(fb->buf, fb->len);
    f.close();
    Serial.printf("saved %s (%u bytes)\n", path, (unsigned)fb->len);
  } else {
    Serial.println("SD write failed");
  }
  esp_camera_fb_return(fb);

  delay(10000);
}
