// BatteryManager.h
// STATUS: 3 NEEDS EXTRA HARDWARE (interface is 1 REAL) — same situation as
// the XIAO's battery (docs/hardware.md §2.5): no fixed circuit was defined
// for this board's battery either, so this never invents a percentage.

#pragma once
#include <Arduino.h>

struct BatteryReading {
  bool hasReading = false;
  float voltage = 0.0f;
  int percent = -1;
};

class BatteryManager {
public:
  void begin(bool enableAdcDivider, int adcPin = -1); // -1 = board's own A0 macro
  BatteryReading read();
  static bool isLow(const BatteryReading& r) { return r.hasReading && r.percent <= 15; }

private:
  bool _enabled = false;
  int _adcPin = A0;
  static constexpr float kDividerRatio = 2.0f;
  static constexpr float kEmptyVoltage = 3.30f;
  static constexpr float kFullVoltage  = 4.20f;
};
