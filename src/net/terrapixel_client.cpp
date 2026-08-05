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

bool TerraPixelClient::refreshStatus()
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

bool TerraPixelClient::setFilmMode(bool on)
{
    // terraPixel's /set treats "film" as an HTML checkbox: its mere
    // presence means on, and -- critically -- its ABSENCE always resets
    // filmMode to false (server.hasArg("film") is unconditional, unlike
    // bright/radius which are only touched when present). So every /set
    // call must resend the other two fields from our cached status_ to
    // avoid silently clobbering them.
    String body = "bright=" + String(status_.brightness) + "&radius=" + String(status_.radius, 1);
    if (on) body += "&film=on";
    bool ok = postForm("/set", body);
    if (ok) status_.filmMode = on;
    return ok;
}

bool TerraPixelClient::setBrightness(uint8_t percent)
{
    String body = "bright=" + String(percent) + "&radius=" + String(status_.radius, 1);
    if (status_.filmMode) body += "&film=on";
    bool ok = postForm("/set", body);
    if (ok) status_.brightness = percent;
    return ok;
}

bool TerraPixelClient::setRadius(float radiusLeds)
{
    String body = "bright=" + String(status_.brightness) + "&radius=" + String(radiusLeds, 1);
    if (status_.filmMode) body += "&film=on";
    bool ok = postForm("/set", body);
    if (ok) status_.radius = radiusLeds;
    return ok;
}

bool TerraPixelClient::toggleParty()
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
