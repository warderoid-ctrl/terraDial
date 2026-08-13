#include "terrapixel_client.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include "../config/settings.h"

TerraPixelClient terraPixel;

void TerraPixelClient::begin()
{
    // mDNS is started once in main.cpp (shared with FluidNCClient) before
    // this is used -- nothing to do here.
}

bool TerraPixelClient::ensureResolved()
{
    if (haveIp_) return true;

    uint32_t now = millis();
    if (now - lastResolveAttempt_ < 3000) return false;
    lastResolveAttempt_ = now;

    IPAddress ip = MDNS.queryHost(Config::get().terraPixelHost, 1500);
    if (ip == IPAddress((uint32_t)0)) return false;

    resolvedIp_ = ip;
    haveIp_ = true;
    Serial.printf("[terrapixel] resolved %s.local -> %s\n", Config::get().terraPixelHost, ip.toString().c_str());
    return true;
}

String TerraPixelClient::baseUrl()
{
    return "http://" + resolvedIp_.toString();
}

bool TerraPixelClient::refreshStatusNow()
{
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!ensureResolved())
    {
        status_.reachable = false;
        return false;
    }

    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);
    if (!http.begin(baseUrl() + "/status"))
    {
        status_.reachable = false;
        return false;
    }

    int code = http.GET();
    if (code != 200)
    {
        http.end();
        status_.reachable = false;
        haveIp_ = false; // in case terraPixel's IP changed, re-resolve next time
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err)
    {
        status_.reachable = false;
        return false;
    }

    status_.filmMode = doc["filmMode"] | false;
    status_.brightness = doc["brightness"] | status_.brightness;
    status_.radius = doc["radius"] | status_.radius;
    status_.party = doc["party"] | false;
    const char *mode = doc["mode"] | "IDLE";
    strncpy(status_.mode, mode, sizeof(status_.mode) - 1);
    status_.mode[sizeof(status_.mode) - 1] = '\0';
    status_.reachable = true;

    // The UI's controls start from whatever terraPixel already had, rather
    // than from this firmware's defaults -- otherwise the first slider touch
    // would push our guesses onto a device that was already configured.
    if (!seededDesired_)
    {
        desiredFilm_ = status_.filmMode;
        desiredBrightness_ = status_.brightness;
        desiredRadius_ = status_.radius;
        seededDesired_ = true;
    }
    return true;
}

bool TerraPixelClient::postForm(const String &path, const String &body)
{
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!ensureResolved()) return false;

    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS); // /set replies 303; we only care it was accepted
    if (!http.begin(baseUrl() + path)) return false;
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int code = http.POST(body);
    http.end();

    if (code < 200 || code >= 400)
    {
        haveIp_ = false;
        return false;
    }
    return true;
}

bool TerraPixelClient::applySetNow()
{
    // terraPixel's /set treats "film" as an HTML checkbox: its mere presence
    // means on, and -- critically -- its ABSENCE always resets filmMode to
    // false (server.hasArg("film") is unconditional, unlike bright/radius
    // which are only touched when present). So every /set must carry all
    // three fields or it silently clobbers the others.
    bool film = desiredFilm_;
    uint8_t bright = desiredBrightness_;
    float radius = desiredRadius_;

    String body = "bright=" + String(bright) + "&radius=" + String(radius, 1);
    if (film) body += "&film=on";
    if (!postForm("/set", body)) return false;

    status_.filmMode = film;
    status_.brightness = bright;
    status_.radius = radius;
    return true;
}

bool TerraPixelClient::togglePartyNow()
{
    if (WiFi.status() != WL_CONNECTED) return false;
    if (!ensureResolved()) return false;

    HTTPClient http;
    http.setConnectTimeout(REQUEST_TIMEOUT_MS);
    http.setTimeout(REQUEST_TIMEOUT_MS);
    if (!http.begin(baseUrl() + "/party")) return false;

    int code = http.POST("");
    if (code != 200)
    {
        http.end();
        haveIp_ = false;
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) return false;

    status_.party = doc["party"] | status_.party;
    return true;
}

// ---- UI-thread side: record intent only, never touch the network ----

void TerraPixelClient::setFilmMode(bool on)
{
    desiredFilm_ = on;
    setDirty_ = true;
}

void TerraPixelClient::setBrightness(uint8_t value)
{
    desiredBrightness_ = value;
    setDirty_ = true;
}

void TerraPixelClient::setRadius(float radiusLeds)
{
    desiredRadius_ = radiusLeds;
    setDirty_ = true;
}

void TerraPixelClient::toggleParty() { partyPending_ = true; }
void TerraPixelClient::requestRefresh() { refreshPending_ = true; }

// ---- networkTask side: everything that can block ----

void TerraPixelClient::update()
{
    if (WiFi.status() != WL_CONNECTED) return;

    // Settings first, so a slider drag lands before the next status poll
    // reads back a stale value.
    if (setDirty_)
    {
        setDirty_ = false;
        applySetNow();
    }

    if (partyPending_)
    {
        partyPending_ = false;
        togglePartyNow();
    }

    uint32_t now = millis();
    if (refreshPending_ || now - lastRefreshAt_ >= REFRESH_MS)
    {
        refreshPending_ = false;
        lastRefreshAt_ = now;
        refreshStatusNow();
    }
}
