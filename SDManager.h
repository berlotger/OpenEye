// SDManager.h
// STATUS: 1 REAL — SD.begin(21) SPI mode, verified against Seeed's own
// tutorials (docs/hardware.md §1.2). Note the documented GPIO21 conflict with
// the camera flash LED — this project does not use the flash LED, on purpose.

#pragma once
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <vector>

class SDManager {
public:
  static constexpr int SD_CS_PIN = 21;

  bool begin();
  bool isReady() const { return _ready; }

  // Creates the standard OpenVisionEye folder tree if missing.
  bool ensureDirectoryStructure();

  bool writeFile(const char* path, const uint8_t* data, size_t len);
  bool appendFile(const char* path, const uint8_t* data, size_t len);
  File openForWrite(const char* path);   // caller closes it
  File openForRead(const char* path);    // caller closes it
  bool exists(const char* path);

  // Returns a fresh, non-colliding filename like "/OpenVisionEye/photos/IMG_0007.jpg"
  String nextIndexedFilename(const char* folder, const char* prefix, const char* ext);

  // Lists regular files (not sub-folders) directly inside `folder`, full paths.
  std::vector<String> listFiles(const char* folder);

private:
  bool _ready = false;
};
