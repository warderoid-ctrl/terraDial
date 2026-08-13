#include "settings.h"
#include <Preferences.h>
#include <string.h>
#include "secrets.h"
#include "jog_config.h"

namespace
{
    Preferences prefs;
    AppSettings settings;

    void copyToBuf(char *dest, size_t destSize, const String &s)
    {
        strncpy(dest, s.c_str(), destSize - 1);
        dest[destSize - 1] = '\0';
    }
}

namespace Config
{
    void begin()
    {
        prefs.begin("terratouch", false);

        copyToBuf(settings.fluidNcHost, sizeof(settings.fluidNcHost), prefs.getString("fncHost", FLUIDNC_HOST));
        settings.sleepTimeoutSec = prefs.getUShort("sleepSec", 300); // 5 min
        settings.backlightBrightnessPct = prefs.getUChar("blBright", 100);
        settings.sleepLedBrightnessPct = prefs.getUChar("sleepLed", 50);
        settings.invertMenuRotation = prefs.getBool("invMenuRot", false);
        settings.penJogMm = prefs.getFloat("penMm", PEN_JOG_MM);
        settings.penJogFeed = prefs.getFloat("penFeed", PEN_JOG_FEED);
        settings.railFilmMode = prefs.getBool("railFilm", false);
        settings.railFilmBrightness = prefs.getUChar("railBright", 200);
        settings.railCometRadius = prefs.getFloat("railComet", 3.5f);
        settings.railSleepMode = prefs.getUChar("railSleep", RAIL_SLEEP_ON);
        settings.railSleepBrightnessPct = prefs.getUChar("railSleepB", 40);
        copyToBuf(settings.wifiSsid, sizeof(settings.wifiSsid), prefs.getString("wifiSsid", WIFI_SSID));
        copyToBuf(settings.wifiPass, sizeof(settings.wifiPass), prefs.getString("wifiPass", WIFI_PASS));
    }

    AppSettings &get() { return settings; }

    void save()
    {
        prefs.putString("fncHost", settings.fluidNcHost);
        prefs.putUShort("sleepSec", settings.sleepTimeoutSec);
        prefs.putUChar("blBright", settings.backlightBrightnessPct);
        prefs.putUChar("sleepLed", settings.sleepLedBrightnessPct);
        prefs.putBool("invMenuRot", settings.invertMenuRotation);
        prefs.putFloat("penMm", settings.penJogMm);
        prefs.putFloat("penFeed", settings.penJogFeed);
        prefs.putBool("railFilm", settings.railFilmMode);
        prefs.putUChar("railBright", settings.railFilmBrightness);
        prefs.putFloat("railComet", settings.railCometRadius);
        prefs.putUChar("railSleep", settings.railSleepMode);
        prefs.putUChar("railSleepB", settings.railSleepBrightnessPct);
        prefs.putString("wifiSsid", settings.wifiSsid);
        prefs.putString("wifiPass", settings.wifiPass);
    }
}
