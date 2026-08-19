// ConfigManager.cpp
// STATUS: 1 REAL — requires ArduinoJson (see requirements.md)

#include "ConfigManager.h"
#include <ArduinoJson.h>
#include <SD.h>

bool ConfigManager::begin() {
  if (SD.exists(kPath)) {
    if (loadFromDisk()) return true;
    Serial.println("[ConfigManager] existing config.json failed to parse, rewriting defaults");
  }
  return writeDefaults();
}

bool ConfigManager::loadFromDisk() {
  File f = SD.open(kPath, FILE_READ);
  if (!f) return false;

  JsonDocument doc; // ArduinoJson v7 style
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[ConfigManager] JSON parse error: %s\n", err.c_str());
    return false;
  }

  _config.language = doc["language"] | "en";
  _config.volume = doc["volume"] | 70;
  _config.wakeWordEnabled = doc["wakeWordEnabled"] | true;
  _config.audioMode = doc["audioMode"] | "wifi_to_bt";
  _config.batteryAdcDividerEnabled = doc["batteryAdcDividerEnabled"] | false;
  _config.batteryAdcPin = doc["batteryAdcPin"] | -1;
  _config.apSsidPrefix = doc["apSsidPrefix"] | "OpenVisionEye";
  _config.apPassword = doc["apPassword"] | "glasses1234";

  Serial.println("[ConfigManager] loaded config.json");
  return true;
}

bool ConfigManager::writeDefaults() {
  _config = OpenVisionEyeConfig{}; // struct defaults
  return save();
}

bool ConfigManager::save() {
  JsonDocument doc;
  doc["language"] = _config.language;
  doc["volume"] = _config.volume;
  doc["wakeWordEnabled"] = _config.wakeWordEnabled;
  doc["audioMode"] = _config.audioMode;
  doc["batteryAdcDividerEnabled"] = _config.batteryAdcDividerEnabled;
  doc["batteryAdcPin"] = _config.batteryAdcPin;
  doc["apSsidPrefix"] = _config.apSsidPrefix;
  doc["apPassword"] = _config.apPassword;

  File f = SD.open(kPath, FILE_WRITE);
  if (!f) {
    Serial.println("[ConfigManager] could not open config.json for write");
    return false;
  }
  bool ok = serializeJsonPretty(doc, f) > 0;
  f.close();
  return ok;
}

void ConfigManager::setVolume(int v) {
  _config.volume = constrain(v, 0, 100);
  save();
}
