// ProtocolDefs.h
// STATUS: 1 REAL
//
// Shared string constants for the Wi-Fi control protocol described in
// docs/wifi_protocol.md. Kept identical (copy-pasted on purpose — see
// ESP32_Audio/ProtocolDefs.h for why) on both boards so a typo in one place
// doesn't silently desync the two firmwares.

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
  // Prefixed commands (parse with startsWith + substring):
  //   "SAY:<text>"
  //   "VOLUME:<0-100>"
  //   "BATTERY:<0-100 or UNKNOWN>"
  constexpr const char* PREFIX_SAY      = "SAY:";
  constexpr const char* PREFIX_VOLUME   = "VOLUME:";
  constexpr const char* PREFIX_BATTERY  = "BATTERY:";
}
