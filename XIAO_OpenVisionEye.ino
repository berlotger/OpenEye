// XIAO_OpenVisionEye.ino
// STATUS: 1 REAL (orchestrates the modules; see each module's own STATUS
// comment for what's real vs. stub vs. future).
//
// Board: Seeed Studio XIAO ESP32-S3 Sense
// Arduino IDE board setting: "XIAO_ESP32S3" (search "xiao s3" in Boards Manager)
// PSRAM: OPI PSRAM (required — camera needs it)
// See docs/installation.md for the full Arduino IDE setup.
//
// ---- v1.2 AI Offline: two EXPERIMENTAL, OFF-BY-DEFAULT feature flags ----
// Both are commented out on purpose. Read MultiNetSTT.h and VisionAI.h
// (and docs/installation.md) before enabling either — neither is confirmed
// to work on this exact 8MB-flash board without your own hardware testing.
// #define OVE_ENABLE_MULTINET       // Part 1: offline voice commands (ESP-SR/MultiNet)
// #define OVE_ENABLE_EDGE_IMPULSE   // Part 2: offline object detection (needs YOUR trained model)

#include "SDManager.h"
#include "CameraManager.h"
#include "AudioManager.h"
#include "AudioBackend.h"
#include "BatteryManager.h"
#include "WakeWordEngine.h"
#include "ButtonManager.h"
#include "WiFiManager.h"
#include "ConfigManager.h"
#include "CommandManager.h"
#include "VisionAI.h"
#include "LanguageAI.h"
#include "MultiNetSTT.h"

#define BOOT_BUTTON_PIN 0

SDManager sdManager;
CameraManager cameraManager;
AudioManager audioManager;
BatteryManager batteryManager;
ManualTriggerWakeWord wakeWord(BOOT_BUTTON_PIN);
ButtonManager buttonManager; // GPIO2 / "D1" — see ButtonManager.h for why
WiFiManager wifiManager;
ConfigManager configManager;
LocalAudioBackend localAudioBackend(&wifiManager);
PhoneAudioBackend phoneAudioBackend; // NOT IMPLEMENTED — see AudioBackend.h
AudioBackendManager audioBackendManager(&localAudioBackend, &phoneAudioBackend);
NullVisionAI visionAI;
TemplateLanguageAI languageAI;
CommandManager commandManager;
MultiNetSTT multiNetStt; // no-op unless OVE_ENABLE_MULTINET is defined AND voiceMode=MULTINET — see MultiNetSTT.h
bool multiNetActive = false;

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

  buttonManager.setMapping(
    buttonActionFromString(configManager.get().buttonSingleAction),
    buttonActionFromString(configManager.get().buttonDoubleAction),
    buttonActionFromString(configManager.get().buttonLongAction));
  buttonManager.begin();

  bool wifiOk = wifiManager.begin(configManager.get().apSsidPrefix, configManager.get().apPassword);

  localAudioBackend.begin();
  phoneAudioBackend.begin();
  audioBackendManager.setMode(AudioBackendManager::modeFromString(configManager.get().audioBackendMode));

  commandManager.begin(&wakeWord, &cameraManager, &sdManager, &audioManager,
                        &audioBackendManager, &batteryManager, &wifiManager,
                        &configManager, &visionAI, &languageAI, &buttonManager);

  wifiManager.setControlLineHandler([](const String& line) {
    commandManager.handleControlLine(line);
  });

  // v1.2 AI Offline: only attempt to start MultiNet if the person has both
  // (a) compiled with OVE_ENABLE_MULTINET defined (see top of this file)
  // AND (b) set voiceMode=MULTINET in config.json (via the VOICE_MODE:
  // command — see CommandManager.cpp). Neither alone does anything. See
  // MultiNetSTT.h for exactly why this stays experimental.
#ifdef OVE_ENABLE_MULTINET
  if (configManager.get().voiceMode == "MULTINET") {
    multiNetActive = multiNetStt.begin();
  }
#endif

  bool psramOk = psramFound();

  Serial.println("---- DIAGNOSTIC MODE ----");
  Serial.printf("[%s] Camera\n", cameraOk ? "OK" : "FAIL");
  Serial.printf("[%s] SD\n", sdOk ? "OK" : "FAIL");
  Serial.printf("[%s] Wi-Fi AP\n", wifiOk ? "OK" : "FAIL");
  Serial.printf("[%s] Microphone\n", micOk ? "OK" : "FAIL");
  Serial.printf("[%s] PSRAM detected\n", psramOk ? "OK" : "FAIL");
  Serial.printf("[%s] Battery (%s)\n",
    configManager.get().batteryAdcDividerEnabled ? "OK" : "N/A",
    configManager.get().batteryAdcDividerEnabled ? "circuit configured" : "no circuit configured, see docs/hardware.md");
  Serial.println("[--] Audio ESP32 — will report once it connects over Wi-Fi");
  Serial.printf("[SKIP] Button (GPIO%d) — wiring not verified in this environment, see docs/hardware.md\n", ButtonManager::DEFAULT_BUTTON_GPIO);
  Serial.printf("[--] Audio backend mode: %s\n", configManager.get().audioBackendMode.c_str());
  Serial.printf("[--] \"Jarvis\" wake-word stub (optional/legacy): %s\n",
    configManager.get().wakeWordEnabled ? "ENABLED" : "disabled (default) — see docs/architecture.md");
#ifdef OVE_ENABLE_MULTINET
  Serial.println("[OK] ESP-SR library compiled in (OVE_ENABLE_MULTINET defined)");
  if (configManager.get().voiceMode == "MULTINET") {
    Serial.printf("[%s] MultiNet model loaded\n", multiNetActive ? "OK" : "FAIL");
    if (!multiNetActive) Serial.println("      -> see docs/architecture.md \"Part 1 - offline voice command recognition\"");
  } else {
    Serial.println("[SKIP] MultiNet model loaded — voiceMode is OFF (set VOICE_MODE:MULTINET + reboot to try it)");
  }
#else
  Serial.println("[SKIP] ESP-SR available — built without OVE_ENABLE_MULTINET, see top of this file");
  Serial.println("[SKIP] MultiNet model loaded — ESP-SR not compiled in");
#endif
#ifdef OVE_ENABLE_EDGE_IMPULSE
  Serial.println("[OK] Edge Impulse library compiled in (OVE_ENABLE_EDGE_IMPULSE defined)");
  Serial.println("[SKIP] Vision inference test — run Vision_Test.ino standalone first, see examples/Vision_Test");
#else
  Serial.println("[SKIP] Edge Impulse model loaded — built without OVE_ENABLE_EDGE_IMPULSE, see top of this file");
  Serial.println("[SKIP] Vision inference test — Edge Impulse not compiled in");
#endif
  Serial.println("-------------------------");

  if (wifiOk) {
    Serial.println("SSID: " + wifiManager.ssid());
    Serial.println("AP IP: " + wifiManager.apIpString());
  }

  Serial.println("Ready.");
  Serial.println("Primary control: long-press the button (GPIO2/\"D1\") to enter command mode,");
  Serial.println("then type a command — e.g. \"take a photo\", \"record audio\", \"battery\".");
  Serial.println("Single-click = photo, double-click = video start/stop (see docs/hardware.md).");
  Serial.println("No button wired yet? Type HEY_GLASSES on the Serial Monitor to open the same");
  Serial.println("command-listening window, then type your command.");
  Serial.printf("(Optional legacy path: the \"Jarvis\" wake-word stub is %s — see config.json's\n",
    configManager.get().wakeWordEnabled ? "ENABLED" : "disabled by default in this version");
  Serial.println(" wakeWordEnabled. JARVIS/HEY_JARVIS/HEY_AI text shortcuts still work either way.)");
  Serial.println("Type DIAG to re-run diagnostics at any time.");
  Serial.println("Type AUDIO_TEST to send a 440Hz test tone through the full Wi-Fi -> ESP32 Audio -> A2DP pipeline.");
  Serial.println("Type PING_TEST to measure round-trip time to the Audio hub.");
  Serial.println("Type VISION_MODE:LOCAL or VOICE_MODE:MULTINET to opt into the v1.2 EXPERIMENTAL AI");
  Serial.println("engines (both need a firmware rebuild with the matching OVE_ENABLE_* flag first —");
  Serial.println("see the top of this file and docs/architecture.md).");
}

void loop() {
  wifiManager.loop();
  commandManager.loop();

  ButtonClick click = buttonManager.poll();
  if (click != ButtonClick::NONE) {
    ButtonAction action = buttonManager.actionFor(click);
    Serial.printf("[Button] click=%d -> action=%s\n", (int)click, buttonActionToString(action).c_str());
    commandManager.handleButtonAction(action);
  }

  // v1.2 AI Offline: only polled when MultiNet actually started successfully
  // (see setup()) — multiNetStt.poll() is a safe no-op otherwise either way.
  if (multiNetActive) {
    VoiceCommandId voiceId;
    if (multiNetStt.poll(voiceId)) {
      commandManager.handleVoiceCommand(voiceId);
    }
  }

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
  Serial.printf("[%s] Wi-Fi AP (%s)\n", wifiManager.isApActive() ? "OK" : "FAIL", wifiManager.ssid().c_str());
  Serial.printf("[%s] Audio hub link\n", wifiManager.controlClientConnected() ? "OK" : "WAITING");
  BatteryReading r = batteryManager.read();
  Serial.printf("[%s] Battery %s\n", r.hasReading ? "OK" : "N/A",
    r.hasReading ? (String(r.percent) + "%").c_str() : "not configured");
  Serial.printf("[--] Audio backend: %s (mode=%s)\n", audioBackendManager.active()->name(),
    configManager.get().audioBackendMode.c_str());
  Serial.printf("[--] \"Jarvis\" wake-word stub (optional/legacy): %s\n",
    configManager.get().wakeWordEnabled ? "ENABLED" : "disabled (default)");
  Serial.println("[SKIP] Button — wiring not verified in this environment, see docs/hardware.md");
#ifdef OVE_ENABLE_MULTINET
  Serial.println("[OK] ESP-SR available (OVE_ENABLE_MULTINET compiled in)");
  if (configManager.get().voiceMode == "MULTINET") {
    Serial.printf("[%s] MultiNet model loaded\n", multiNetActive ? "OK" : "FAIL");
  } else {
    Serial.println("[SKIP] MultiNet model loaded — voiceMode is OFF");
  }
#else
  Serial.println("[SKIP] ESP-SR available — built without OVE_ENABLE_MULTINET");
  Serial.println("[SKIP] MultiNet model loaded — ESP-SR not compiled in");
#endif
#ifdef OVE_ENABLE_EDGE_IMPULSE
  Serial.println("[OK] Edge Impulse library compiled in (OVE_ENABLE_EDGE_IMPULSE)");
  Serial.println("[SKIP] Vision inference test — run examples/Vision_Test standalone first");
#else
  Serial.println("[SKIP] Edge Impulse model loaded — built without OVE_ENABLE_EDGE_IMPULSE");
  Serial.println("[SKIP] Vision inference test — Edge Impulse not compiled in");
#endif
  Serial.println("-------------------------");
  return true;
}
