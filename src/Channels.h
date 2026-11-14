#pragma once

/*
  WheelStat - src/Channels.h
  ===========================================================================
  The tables everything else loops over. Every measured quantity is an
  always-positive CHANNEL (signed ones are split in two, so left and right
  keep separate maxima); window minimum, interval and session maxima,
  summary columns and all-time records are all loops over this table. Adding
  a quantity takes one enum entry, one table row and one line in
  stats::sample().

  No Arduino dependency: src/core includes it too.
  ===========================================================================
*/

#include <stdint.h>

enum Channel : uint8_t {
  CH_LEAN_RIGHT,   // degrees of lean to the right
  CH_LEAN_LEFT,    // degrees of lean to the left
  CH_WHEELIE,      // degrees nose up
  CH_STOPPIE,      // degrees nose down
  CH_GLAT_RIGHT,   // lateral G to the right
  CH_GLAT_LEFT,    // lateral G to the left
  CH_G_ACCEL,      // G under acceleration
  CH_G_BRAKE,      // G under braking
  CH_SPEED,        // km/h from the GPS (0 without a valid fix)
  CHANNEL_COUNT
};

struct ChannelInfo {
  const char *csvName;   // CSV column, language independent
  const char *label;     // human label for the website, localised
  const char *unitHtml;  // unit suffix as HTML
  uint8_t     decimals;  // digits in the CSV summary and on the website
};

const ChannelInfo &channelInfo(uint8_t channel);

// Events: manoeuvres counted when the sustained level crosses a threshold.
// To re-arm, the level must drop below threshold * EVENT_REARM_RATIO, so a
// manoeuvre hovering around the threshold counts once. The thresholds are
// settings (src/Settings); their defaults and limits are in
// src/core/SettingsRules.
enum Event : uint8_t {
  EV_WHEELIE,
  EV_STOPPIE,
  EV_LEAN,        // either side
  EV_HARD_BRAKE,
  EV_HARD_ACCEL,
  EVENT_COUNT
};

constexpr float EVENT_REARM_RATIO = 0.7f;

struct EventInfo {
  const char *csvName;    // CSV column, language independent
  const char *oledLabel;  // column-aligned label for the OLED summary, localised
  const char *label;      // human label for the website, localised
  bool        inG;        // threshold in G rather than degrees
};

const EventInfo &eventInfo(uint8_t event);
