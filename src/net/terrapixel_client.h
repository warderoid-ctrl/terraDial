#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// Snapshot of terraPixel's state, read from its /status endpoint (added to
// warderoid-ctrl/terraPixel alongside /party -- see that repo's main.cpp).
struct TerraPixelStatus
{
    bool reachable = false; // false until the first successful /status read
    bool filmMode = false;
    uint8_t brightness = 200;
    float radius = 3.5f;
    bool party = false;
    char mode[16] = "BOOT";
};

// Unlike FluidNCClient, this is a plain synchronous HTTP client -- terraPixel
// has no push/streaming interface, only request/response, and the Lights
// screen only needs to talk to it on user action (entering the screen,
// dragging a slider, tapping a toggle), not every loop iteration. A short
// timeout keeps a single call from stalling the UI noticeably if terraPixel
// is unreachable.
class TerraPixelClient
{
public:
    // Call once mDNS is already up (see FluidNCClient/main.cpp).
    void begin();

    // Blocking HTTP calls, ~1s worst case (bounded by REQUEST_TIMEOUT_MS).
    // Each updates status_ from the response (or marks reachable=false on
    // failure) and returns whether it succeeded.
    bool refreshStatus();
    bool setFilmMode(bool on);
    bool setBrightness(uint8_t percent);
    bool setRadius(float radiusLeds);
    bool toggleParty();

    const TerraPixelStatus &status() const { return status_; }

private:
    TerraPixelStatus status_;
    bool haveIp_ = false;
    IPAddress resolvedIp_;
    uint32_t lastResolveAttempt_ = 0;

    static const uint32_t REQUEST_TIMEOUT_MS = 1000;

    bool ensureResolved();
    String baseUrl();
    bool postForm(const String &path, const String &body);
};

extern TerraPixelClient terraPixel;
