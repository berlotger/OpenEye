// SDManager.cpp
// STATUS: 1 REAL

#include "SDManager.h"

static const char* kFolders[] = {
  "/OpenVisionEye",
  "/OpenVisionEye/photos",
  "/OpenVisionEye/videos",
  "/OpenVisionEye/audio",
  "/OpenVisionEye/music",
  "/OpenVisionEye/models",
  "/OpenVisionEye/logs",
  "/OpenVisionEye/config",
};

bool SDManager::begin() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[SDManager] mount failed");
    _ready = false;
    return false;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SDManager] no SD card attached");
    _ready = false;
    return false;
  }
  _ready = true;
  Serial.println("[SDManager] mounted OK");
  return ensureDirectoryStructure();
}

bool SDManager::ensureDirectoryStructure() {
  if (!_ready) return false;
  bool allOk = true;
  for (const char* folder : kFolders) {
    if (!SD.exists(folder)) {
      if (!SD.mkdir(folder)) {
        Serial.printf("[SDManager] failed to create %s\n", folder);
        allOk = false;
      }
    }
  }
  return allOk;
}

bool SDManager::writeFile(const char* path, const uint8_t* data, size_t len) {
  if (!_ready) return false;
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.printf("[SDManager] open for write failed: %s\n", path);
    return false;
  }
  if (data == nullptr || len == 0) {
    // Used to create/truncate an empty file (e.g. starting a new video
    // recording before any frames exist yet) — nothing to write.
    f.close();
    return true;
  }
  size_t written = f.write(data, len);
  f.close();
  if (written != len) {
    Serial.printf("[SDManager] short write on %s (%u/%u)\n", path, (unsigned)written, (unsigned)len);
    return false;
  }
  return true;
}

bool SDManager::appendFile(const char* path, const uint8_t* data, size_t len) {
  if (!_ready) return false;
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  size_t written = f.write(data, len);
  f.close();
  return written == len;
}

File SDManager::openForWrite(const char* path) {
  return SD.open(path, FILE_WRITE);
}

File SDManager::openForRead(const char* path) {
  return SD.open(path, FILE_READ);
}

bool SDManager::exists(const char* path) {
  if (!_ready) return false;
  return SD.exists(path);
}

String SDManager::nextIndexedFilename(const char* folder, const char* prefix, const char* ext) {
  // Zero-padded to 6 digits (e.g. "IMG_000001.jpg") per the naming
  // convention in the project spec, and so filenames sort correctly as
  // plain text on any file browser rather than lexicographically
  // (IMG_2 vs IMG_10) misordering.
  char numBuf[8];
  for (int i = 1; i < 1000000; i++) {
    snprintf(numBuf, sizeof(numBuf), "%06d", i);
    String candidate = String(folder) + "/" + prefix + "_" + numBuf + "." + ext;
    if (!SD.exists(candidate.c_str())) {
      return candidate;
    }
  }
  // Extremely unlikely fallback.
  return String(folder) + "/" + prefix + "_overflow." + ext;
}

std::vector<String> SDManager::listFiles(const char* folder) {
  std::vector<String> files;
  if (!_ready) return files;
  File dir = SD.open(folder);
  if (!dir || !dir.isDirectory()) return files;

  File entry = dir.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      // Depending on the SD library version, entry.name() may return a bare
      // filename or an already-full path. Handle both rather than assuming.
      String name = String(entry.name());
      if (name.startsWith("/")) {
        files.push_back(name);
      } else {
        files.push_back(String(folder) + "/" + name);
      }
    }
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  return files;
}
