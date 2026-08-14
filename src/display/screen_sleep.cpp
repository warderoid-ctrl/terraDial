#include "screen_sleep.h"
#include <Arduino.h>
#include "lgfx_config.h" // backlightSet()
#include "../config/settings.h"
#include "ui_brand.h"
#include "../led/panel_ring.h"

namespace
{
    bool asleep = false;
    uint32_t lastActivityAt = 0;

    // The ring brightness the user actually chose, saved on the way into
    // sleep so waking restores it rather than leaving the ring stuck at the
    // sleep level.
    uint8_t savedLedBrightness = 60;

    // How long before the brand screen appears. Deliberately much shorter
    // than the sleep timeout and not user-configurable: it's a display, not
    // a decision -- one toggle to disable it is enough.
    const uint32_t IDLE_LOGO_MS = 30000;

    void goToSleep()
    {
        asleep = true;
        savedLedBrightness = panelRing.brightness();
        panelRing.setBrightness(Config::get().sleepLedBrightnessPct);
        backlightSet(0); // fully off, not dimmed -- the point is a dark panel
    }

    void wake()
    {
        UiBrand::hide(); // sleep is normally entered via the brand screen
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
        uint32_t idleMs = millis() - lastActivityAt;

        // Stage one: the brand screen. Runs on its own timer so it still
        // appears when sleep is switched off entirely -- an idle panel on
        // the machine may as well show the mark.
        if (!asleep && Config::get().showIdleLogo && idleMs > IDLE_LOGO_MS && !UiBrand::isShown())
            UiBrand::show();

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

        // Both stages swallow the input that dismisses them. Grabbing a dark
        // or branded panel to see what's happening must never also press
        // whatever was underneath.
        bool dismissed = false;
        if (UiBrand::isShown())
        {
            UiBrand::hide();
            dismissed = true;
        }
        if (asleep)
        {
            wake();
            dismissed = true;
        }
        return dismissed;
    }
}
