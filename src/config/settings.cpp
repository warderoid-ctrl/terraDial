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
        copyToBuf(settings.terraPixelHost, sizeof(settings.terraPixelHost), prefs.getString("tpHost", TERRAPIXEL_HOST));
        settings.backlightTimeoutSec = prefs.getUShort("blTimeout", 60);
        settings.backlightBrightnessPct = prefs.getUChar("blBright", 100);
        settings.invertMenuRotation = prefs.getBool("invMenuRot", false);
        settings.penJogMm = prefs.getFloat("penMm", PEN_JOG_MM);
        settings.penJogFeed = prefs.getFloat("penFeed", PEN_JOG_FEED);
        copyToBuf(settings.wifiSsid, sizeof(settings.wifiSsid), prefs.getString("wifiSsid", WIFI_SSID));
        copyToBuf(settings.wifiPass, sizeof(settings.wifiPass), prefs.getString("wifiPass", WIFI_PASS));
    }

    AppSettings &get() { return settings; }

    void save()
    {
        prefs.putString("fncHost", settings.fluidNcHost);
        prefs.putString("tpHost", settings.terraPixelHost);
        prefs.putUShort("blTimeout", settings.backlightTimeoutSec);
        prefs.putUChar("blBright", settings.backlightBrightnessPct);
        prefs.putBool("invMenuRot", settings.invertMenuRotation);
        prefs.putFloat("penMm", settings.penJogMm);
        prefs.putFloat("penFeed", settings.penJogFeed);
        prefs.putString("wifiSsid", settings.wifiSsid);
        prefs.putString("wifiPass", settings.wifiPass);
    }
}
