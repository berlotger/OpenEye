// XIAO_OpenVisionEye.ino
// STATUS: 1 REAL (orchestrates the modules; see each module's own STATUS
// comment for what's real vs. stub vs. future).
//
// Board: Seeed Studio XIAO ESP32-S3 Sense
// Arduino IDE board setting: "XIAO_ESP32S3" (search "xiao s3" in Boards Manager)
// PSRAM: OPI PSRAM (required — camera needs it)
// See docs/installation.md for the full Arduino IDE setup.

#include "SDManager.h"
#include "CameraManager.h"
#include "AudioManager.h"
#include "BatteryManager.h"
#include "WakeWordEngine.h"
#include "WiFiManager.h"
#include "ConfigManager.h"
#include "CommandManager.h"
#include "VisionAI.h"
#include "LanguageAI.h"

#define BOOT_BUTTON_PIN 0

SDManager sdManager;
CameraManager cameraManager;
AudioManager audioManager;
BatteryManager batteryManager;
ManualTriggerWakeWord wakeWord(BOOT_BUTTON_PIN);
WiFiManager wifiManager;
ConfigManager configManager;
NullVisionAI visionAI;
TemplateLanguageAI languageAI;
CommandManager commandManager;

bool runDiagnostics(); // forward decl, defined below

void setup() {
  Serial.begin(115200);
  unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 3000) { delay(10); } // don't hang forever if unplugged

  Serial.println();
  Serial.println("OpenVisionEye starting...");

  bool sdOk = sdManager.begin();
  configManager.begin(); // works even if SD failed — but config.json needs SD
  bool cameraOk = cameraManager.begin();
  bool micOk = audioManager.begin(&sdManager);
  batteryManager.begin(configManager.get().batteryAdcDividerEnabled, configManager.get().batteryAdcPin);
  wakeWord.begin();
  bool wifiOk = wifiManager.begin(configManager.get().apSsidPrefix, configManager.get().apPassword);

  commandManager.begin(&wakeWord, &cameraManager, &sdManager, &audioManager,
                        &batteryManager, &wifiManager, &configManager,
                        &visionAI, &languageAI);

  wifiManager.setControlLineHandler([](const String& line) {
    commandManager.handleControlLine(line);
  });

  Serial.println("---- DIAGNOSTIC MODE ----");
  Serial.printf("[%s] Camera\n", cameraOk ? "OK" : "FAIL");
  Serial.printf("[%s] SD\n", sdOk ? "OK" : "FAIL");
  Serial.printf("[%s] Wi-Fi AP\n", wifiOk ? "OK" : "FAIL");
  Serial.printf("[%s] Microphone\n", micOk ? "OK" : "FAIL");
  Serial.printf("[%s] Battery (%s)\n",
    configManager.get().batteryAdcDividerEnabled ? "OK" : "N/A",
    configManager.get().batteryAdcDividerEnabled ? "circuit configured" : "no circuit configured, see docs/hardware.md");
  Serial.println("[--] Audio ESP32 — will report once it connects over Wi-Fi");
  Serial.println("-------------------------");

  if (wifiOk) {
    Serial.println("SSID: " + wifiManager.ssid());
    Serial.println("AP IP: " + wifiManager.apIpString());
  }

  Serial.println("Ready. Press BOOT briefly for \"Hey Glasses\", hold >0.8s for \"Hey AI\".");
  Serial.println("Or type HEY_GLASSES / HEY_AI on the Serial Monitor, then your command text.");
  Serial.println("Type DIAG to re-run diagnostics at any time.");
}

void loop() {
  wifiManager.loop();
  commandManager.loop();

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length()) {
      if (line.equalsIgnoreCase("DIAG")) {
        runDiagnostics();
      } else {
        commandManager.handleIncomingText(line);
      }
    }
  }

  // Notify the Audio hub once, right after it connects, matching the "first
  // test" flow from the spec.
  static bool announced = false;
  if (wifiManager.controlClientConnected() && !announced) {
    wifiManager.sendControl("SAY:Audio hub connected.");
    announced = true;
  }
  if (!wifiManager.controlClientConnected()) {
    announced = false;
  }
}

bool runDiagnostics() {
  Serial.println("---- DIAGNOSTIC MODE ----");
  Serial.printf("[%s] Camera\n", cameraManager.isReady() ? "OK" : "FAIL");
  Serial.printf("[%s] SD\n", sdManager.isReady() ? "OK" : "FAIL");
  Serial.printf("[%s] Wi-Fi AP (%s)\n", "OK", wifiManager.ssid().c_str());
  Serial.printf("[%s] Audio hub link\n", wifiManager.controlClientConnected() ? "OK" : "WAITING");
  BatteryReading r = batteryManager.read();
  Serial.printf("[%s] Battery %s\n", r.hasReading ? "OK" : "N/A",
    r.hasReading ? (String(r.percent) + "%").c_str() : "not configured");
  Serial.println("-------------------------");
  return true;
}
