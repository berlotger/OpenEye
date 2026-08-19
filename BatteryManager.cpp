// BatteryManager.cpp
// STATUS: 3 NEEDS EXTRA HARDWARE — see header.

#include "BatteryManager.h"

void BatteryManager::begin(bool enableAdcDivider, int adcPin) {
  _enabled = enableAdcDivider;
  _adcPin = (adcPin < 0) ? A0 : adcPin;
  if (_enabled) {
    pinMode(_adcPin, INPUT);
    Serial.printf("[BatteryManager] ADC divider enabled on pin %d\n", _adcPin);
  } else {
    Serial.println("[BatteryManager] no battery circuit configured — status will report UNKNOWN");
  }
}

BatteryReading BatteryManager::read() {
  BatteryReading r;
  if (!_enabled) return r;

  uint32_t sumMv = 0;
  const int samples = 16;
  for (int i = 0; i < samples; i++) {
    sumMv += analogReadMilliVolts(_adcPin);
  }
  float pinVolts = (sumMv / (float)samples) / 1000.0f;
  float battVolts = pinVolts * kDividerRatio;

  float pct = (battVolts - kEmptyVoltage) / (kFullVoltage - kEmptyVoltage) * 100.0f;
  pct = constrain(pct, 0.0f, 100.0f);

  r.hasReading = true;
  r.voltage = battVolts;
  r.percent = (int)roundf(pct);
  return r;
}
