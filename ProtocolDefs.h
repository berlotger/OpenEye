// ProtocolDefs.h
// STATUS: 1 REAL
//
// This is an intentional duplicate of
// XIAO_OpenVisionEye/ProtocolDefs.h — each board's sketch folder needs to
// be self-contained for the Arduino IDE (a sketch shouldn't reach outside
// its own folder for a #include, since that breaks in some IDE/library-
// manager workflows). If you change one copy when extending the protocol
// (docs/wifi_protocol.md), change the other to match — that's a deliberate
// tradeoff for two small, independently-buildable sketches over a shared
// library that would need its own install step.

#pragma once

namespace Proto {
  constexpr uint16_t CONTROL_PORT = 3333;
  constexpr uint16_t AUDIO_PORT   = 3334;

  constexpr const char* CMD_PLAY           = "PLAY";
  constexpr const char* CMD_PAUSE          = "PAUSE";
  constexpr const char* CMD_STOP           = "STOP";
  constexpr const char* CMD_NEXT           = "NEXT";
  constexpr const char* CMD_PREVIOUS       = "PREVIOUS";
  constexpr const char* CMD_BATTERY        = "BATTERY";
  constexpr const char* CMD_PHOTO_TAKEN    = "PHOTO_TAKEN";
  constexpr const char* CMD_VIDEO_STARTED  = "VIDEO_STARTED";
  constexpr const char* CMD_VIDEO_STOPPED  = "VIDEO_STOPPED";
  constexpr const char* CMD_AUDIO_START    = "AUDIO_START";
  constexpr const char* CMD_AUDIO_END      = "AUDIO_END";
  constexpr const char* CMD_PING           = "PING";
  constexpr const char* CMD_PONG           = "PONG";
  constexpr const char* PREFIX_SAY      = "SAY:";
  constexpr const char* PREFIX_VOLUME   = "VOLUME:";
  constexpr const char* PREFIX_BATTERY  = "BATTERY:";
}
