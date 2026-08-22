// VoiceCommandIds.h
// STATUS: 1 REAL (it's just an enum) — used by the 4 EXPERIMENTAL MultiNet
// integration (MultiNetSTT.h) and by CommandManager, so a recognized speech
// command and a recognized Serial/TCP text command land in the exact same
// place. See docs/architecture.md "Part 1 — offline voice command
// recognition" for what is and isn't verified about the engine that
// produces these IDs.
//
// These ARE the "Command ID" your spec asked for
// (TAKE PHOTO -> COMMAND_PHOTO -> CameraManager.capture(), etc.). The
// values double as the `command_id` field of ESP-SR's `sr_cmd_t` table
// (see MultiNetSTT.cpp) — MultiNet natively recognizes a fixed command
// *identity*, not free text, so there is no separate "convert phrase to
// text" step, exactly as your spec requested.

#pragma once

enum class VoiceCommandId : int {
  NONE             = -1,
  TAKE_PHOTO       = 0,
  START_VIDEO      = 1,
  STOP_VIDEO       = 2,
  ANALYZE          = 3,
  WHAT_DO_YOU_SEE  = 4,
  BATTERY_STATUS   = 5,
  RECORD_AUDIO     = 6,
  STOP_RECORDING   = 7,
  PLAY_MUSIC       = 8,
  STOP_MUSIC       = 9,
};

// Human-readable label, used in Serial logs / diagnostics only.
inline const char* voiceCommandIdToString(VoiceCommandId id) {
  switch (id) {
    case VoiceCommandId::TAKE_PHOTO:      return "TAKE_PHOTO";
    case VoiceCommandId::START_VIDEO:     return "START_VIDEO";
    case VoiceCommandId::STOP_VIDEO:      return "STOP_VIDEO";
    case VoiceCommandId::ANALYZE:         return "ANALYZE";
    case VoiceCommandId::WHAT_DO_YOU_SEE: return "WHAT_DO_YOU_SEE";
    case VoiceCommandId::BATTERY_STATUS:  return "BATTERY_STATUS";
    case VoiceCommandId::RECORD_AUDIO:    return "RECORD_AUDIO";
    case VoiceCommandId::STOP_RECORDING:  return "STOP_RECORDING";
    case VoiceCommandId::PLAY_MUSIC:      return "PLAY_MUSIC";
    case VoiceCommandId::STOP_MUSIC:      return "STOP_MUSIC";
    default:                              return "NONE";
  }
}
