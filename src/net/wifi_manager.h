#pragma once

// Owns WiFi connection lifecycle -- previously an ad hoc static latch
// inside main.cpp's loop(). Centralized so the Settings screen's Wi-Fi
// card has a clean hook to reconnect onto newly-entered credentials
// without reaching into main.cpp's internals.
namespace WifiManager
{
    // Call once from setup(), after Config::begin() (needs the stored/
    // default SSID+password).
    void begin();

    // Call every loop iteration. Non-blocking: once WiFi first connects,
    // starts mDNS and begins fluidNC/terraPixel exactly once.
    void update();

    // True once the one-time mDNS/client-begin step above has run --
    // callers use this to gate fluidNC.update()/terraPixel calls the same
    // way main.cpp's old `fluidNcBegun` latch did.
    bool isReady();

    // Disconnects and reconnects with new credentials (e.g. from the
    // Settings Wi-Fi card), resetting the ready-latch so fluidNC/
    // terraPixel re-begin once the new network connects.
    void reconnect(const char *ssid, const char *pass);
}
