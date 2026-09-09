#pragma once

#include <stdint.h>

// On-device firmware update, straight from the project's GitHub releases.
//
// The panel is already on Wi-Fi and already has two app slots (see
// platformio.ini), so an end user shouldn't need a toolchain, a cable or a
// laptop to take a new build -- Settings > About > Check for updates, and
// the next boot is the new firmware.
//
// Threading, and why it looks like this: every call here is HTTPS and
// blocks for seconds (the download for a minute or more). That's the same
// contract as the FluidNC and terraPixel clients, so it lives the same
// place they do -- update() is called ONLY from networkTask on core 0, and
// the UI never calls anything but the request*/state readers below. Nothing
// in this file may touch LVGL.
//
// The state readers are plain reads of statics written by the network task,
// matching how fluidNC.status() is already consumed across the two cores: a
// UI frame can read a value mid-change, which costs at worst one stale
// label for 150ms.
// The release asset the panel flashes. Fixed rather than version-stamped
// so the updater never has to guess a filename from a tag -- the release
// workflow publishes it under exactly this name.
#define OTA_ASSET_NAME "terradial-ota.bin"

namespace OtaUpdater
{
    enum class State
    {
        Idle,        // nothing has been asked for yet
        Checking,    // querying the releases API
        UpToDate,    // checked, and this build is current
        Available,   // checked, and there's a newer release -- latestVersion() names it
        Installing,  // downloading and flashing; progressPct() moves
        Done,        // flashed successfully, waiting on a restart
        Failed       // message() says why
    };

    // Asked for by the UI; picked up by the next update() on the network
    // task. Both are no-ops if something is already in flight.
    void requestCheck();
    void requestInstall();

    // Clears a Failed/UpToDate result back to Idle so the card stops
    // showing a stale outcome.
    void dismiss();

    // Call from networkTask only, every iteration. Returns immediately
    // unless there's a request outstanding.
    void update();

    State state();
    int progressPct();            // 0-100, meaningful during Installing
    const char *latestVersion();  // "" until a check has found one
    const char *message();        // human-readable status or failure reason

    // True once a successful install is waiting on ESP.restart().
    bool restartPending();
}
