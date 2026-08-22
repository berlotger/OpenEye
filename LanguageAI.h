// LanguageAI.h
// STATUS: 5 FUTURE for a real model. The shipped TemplateLanguageAI is
// 1 REAL but intentionally trivial — see docs/architecture.md "Language AI
// reality check". An on-device LLM was investigated and is not realistic:
// the XIAO ESP32-S3 has 8MB PSRAM / 8MB flash total, one to two orders of
// magnitude below what even the smallest usable instruction-following
// quantized LLMs need for weights + KV cache. This is a hardware ceiling,
// not a missing library — don't try to force one on.

#pragma once
#include <Arduino.h>
#include "VisionAI.h"

class LanguageAI {
public:
  virtual ~LanguageAI() = default;
  virtual String describe(const VisionResult& vision) = 0;
};

// Real, honest, template-based sentence builder. Produces a real sentence
// when VisionAI actually returns data; produces an honest "nothing to
// describe" sentence otherwise (matches NullVisionAI's default in v1).
class TemplateLanguageAI : public LanguageAI {
public:
  String describe(const VisionResult& v) override {
    if (!v.available || v.label.length() == 0) {
      return "I can't see anything to describe right now.";
    }
    String pos = "somewhere";
    switch (v.position) {
      case Position::LEFT: pos = "to the left"; break;
      case Position::CENTER: pos = "ahead"; break;
      case Position::RIGHT: pos = "to the right"; break;
      default: break;
    }
    String depth = "";
    switch (v.depth) {
      case Depth::VERY_NEAR: depth = "very close"; break;
      case Depth::NEAR: depth = "close by"; break;
      case Depth::MEDIUM: depth = "at a moderate distance"; break;
      case Depth::FAR: depth = "in the distance"; break;
      default: break;
    }
    String sentence = "I see a " + v.label;
    if (v.size.length()) sentence += " (" + v.size + ")";
    if (v.color.length()) sentence += ", " + v.color;
    sentence += ", " + pos;
    if (depth.length()) sentence += ", " + depth;
    sentence += ".";
    return sentence;
  }
};
