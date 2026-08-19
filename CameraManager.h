// CameraManager.h
// STATUS: 1 REAL — esp_camera driver, verified pins for
// CAMERA_MODEL_XIAO_ESP32S3 (see docs/hardware.md §1.1).
//
// Photo capture is fully real. "Video" is a best-effort motion-JPEG stream
// written frame-by-frame to an .avi-ish container (concatenated JPEGs), same
// technique Seeed's own tutorials use — there is no on-device video encoder,
// so don't expect a standard playable MP4. This is documented, not hidden.

#pragma once
#include <Arduino.h>
#include "esp_camera.h"

class CameraManager {
public:
  bool begin();                       // true if camera initialized OK
  bool isReady() const { return _ready; }

  // Grabs one frame. Caller must call esp_camera_fb_return(fb) when done.
  camera_fb_t* capture();

  // Convenience: capture + write straight to a path via SDManager caller.
  // Returns the frame so caller can pass fb->buf/fb->len to SDManager.
  // (Kept decoupled from SDManager on purpose — CameraManager should not
  // need to know about the filesystem.)

private:
  bool _ready = false;
};
