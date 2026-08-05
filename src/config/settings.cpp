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
        settings.penJogMm = prefs.getFloat("penMm", PEN_JOG_MM);
        settings.penJogFeed = prefs.getFloat("penFeed", PEN_JOG_FEED);
    }

    AppSettings &get() { return settings; }

    void save()
    {
        prefs.putString("fncHost", settings.fluidNcHost);
        prefs.putString("tpHost", settings.terraPixelHost);
        prefs.putUShort("blTimeout", settings.backlightTimeoutSec);
        prefs.putFloat("penMm", settings.penJogMm);
        prefs.putFloat("penFeed", settings.penJogFeed);
    }
}
