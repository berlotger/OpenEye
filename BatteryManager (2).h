// BatteryManager.h
// STATUS: 3 NEEDS EXTRA HARDWARE (interface is 1 REAL)
//
// The XIAO ESP32-S3 has no documented factory connection from the battery
// pads to any ADC pin. See docs/hardware.md §1.6 for the DIY divider circuit
// this optional implementation expects (200k + 200k to pin A0), and
// docs/installation.md for how to enable it.
//
// This class NEVER invents a battery percentage. If no circuit is configured,
// read() reports hasReading()==false and the rest of the app must say
// "battery status not available" rather than a fake number.

#pragma once
#include <Arduino.h>

struct BatteryReading {
  bool hasReading = false;
  float voltage = 0.0f;   // volts, only valid if hasReading
  int percent = -1;       // 0-100, only valid if hasReading
};

class BatteryManager {
public:
  // enableAdcDivider: set true only after you've actually wired the divider
  // from docs/hardware.md §1.6 — this normally comes from config.json, not
  // a hardcoded true, so a firmware update alone can't start reporting fake
  // numbers on unmodified hardware.
  // adcPin < 0 means "use the board's own A0 macro" (recommended default —
  // see ConfigManager.h for why we don't hardcode a raw GPIO number here).
  void begin(bool enableAdcDivider, int adcPin = -1);

  BatteryReading read();

  static bool isLow(const BatteryReading& r) { return r.hasReading && r.percent <= 15; }

private:
  bool _enabled = false;
  int _adcPin = A0;

  // Calibration for a 1:2 divider (equal resistors) as documented in
  // docs/hardware.md. If your resistor values differ, change the ratio
  // here — do not silently guess a new one at runtime.
  static constexpr float kDividerRatio = 2.0f;      // Vbatt = 2 * Vpin
  static constexpr float kEmptyVoltage = 3.30f;      // LiPo empty (safe cutoff)
  static constexpr float kFullVoltage  = 4.20f;      // LiPo full
};
