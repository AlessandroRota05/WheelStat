#include "Channels.h"

#include "i18n/Text.h"

// Same order as the Channel enum.
static const ChannelInfo CHANNELS[CHANNEL_COUNT] = {
  { "Lean_R",    TR_CH_LEAN_RIGHT, "&deg;",  1 },
  { "Lean_L",    TR_CH_LEAN_LEFT,  "&deg;",  1 },
  { "Wheelie",   TR_CH_WHEELIE,    "&deg;",  1 },
  { "Stoppie",   TR_CH_STOPPIE,    "&deg;",  1 },
  { "GLat_R",    TR_CH_GLAT_RIGHT, " G",     2 },
  { "GLat_L",    TR_CH_GLAT_LEFT,  " G",     2 },
  { "G_Accel",   TR_CH_G_ACCEL,    " G",     2 },
  { "G_Brake",   TR_CH_G_BRAKE,    " G",     2 },
  { "Speed_Kmh", TR_CH_SPEED,      " km/h",  0 },
};

// Same order as the Event enum.
static const EventInfo EVENTS[EVENT_COUNT] = {
  { "Wheelies",   TR_O_EV_WHEELIES, TR_W_EV_WHEELIES, false },
  { "Stoppies",   TR_O_EV_STOPPIES, TR_W_EV_STOPPIES, false },
  { "Leans",      TR_O_EV_LEANS,    TR_W_EV_LEANS,    false },
  { "HardBrakes", TR_O_EV_BRAKES,   TR_W_EV_BRAKES,   true  },
  { "HardAccels", TR_O_EV_ACCELS,   TR_W_EV_ACCELS,   true  },
};

const ChannelInfo &channelInfo(uint8_t channel) {
  return CHANNELS[channel];
}

const EventInfo &eventInfo(uint8_t event) {
  return EVENTS[event];
}
