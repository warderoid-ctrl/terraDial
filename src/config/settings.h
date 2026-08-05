#pragma once

#include <stdint.h>

// Runtime-editable settings, backed by NVS (Preferences, same pattern
// terraPixel already uses). WiFi credentials deliberately stay compile-time
// in secrets.h -- typing an SSID/password on this small round touchscreen
// isn't practical, and it's rare to change. Everything here is loaded once
// at boot (falling back to the compile-time defaults from secrets.h /
// jog_config.h the first time) and saved back to NVS whenever the Settings
// screen changes it.
struct AppSettings
{
    char fluidNcHost[32];
    char terraPixelHost[32];
    uint16_t backlightTimeoutSec; // 0 = never auto-dim
    float penJogMm;
    float penJogFeed; // mm/min
};

namespace Config
{
    void begin();
    AppSettings &get();
    void save();
}
