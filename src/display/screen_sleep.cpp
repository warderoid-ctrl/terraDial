#include "screen_sleep.h"
#include <Arduino.h>
#include "lgfx_config.h" // backlightSet()
#include "../config/settings.h"
#include "../led/panel_ring.h"

namespace
{
    bool asleep = false;
    uint32_t lastActivityAt = 0;

    // The ring brightness the user actually chose, saved on the way into
    // sleep so waking restores it rather than leaving the ring stuck at the
    // sleep level.
    uint8_t savedLedBrightness = 60;

    void goToSleep()
    {
        asleep = true;
        savedLedBrightness = panelRing.brightness();
        panelRing.setBrightness(Config::get().sleepLedBrightnessPct);
        backlightSet(0); // fully off, not dimmed -- the point is a dark panel
    }

    void wake()
    {
        asleep = false;
        backlightSet(Config::get().backlightBrightnessPct);
        panelRing.setBrightness(savedLedBrightness);
        lastActivityAt = millis();
    }
}

namespace ScreenSleep
{
    void begin() { lastActivityAt = millis(); }

    bool isAsleep() { return asleep; }

    void update()
    {
        uint16_t timeoutSec = Config::get().sleepTimeoutSec;
        if (timeoutSec == 0)
        {
            // Sleep switched off while asleep (only reachable if it was
            // changed remotely, but cheap to handle) -- come straight back.
            if (asleep) wake();
            return;
        }
        if (!asleep && millis() - lastActivityAt > (uint32_t)timeoutSec * 1000UL) goToSleep();
    }

    bool noteInputAndWake()
    {
        lastActivityAt = millis();
        if (!asleep) return false;
        wake();
        return true; // caller must swallow this input
    }
}
