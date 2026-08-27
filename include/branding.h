#pragma once

// terraPen project identity, in one place so the About screen and any future
// splash/branding surface agree.
//
// The links are rendered as QR codes on the About screen -- a 240px round
// panel is no place to read a URL, let alone type one, but a phone camera
// handles a QR off the glass fine.
//
// An entry with an empty URL is skipped entirely rather than rendered as a
// dead QR, so it's safe to leave one blank until you have it.
namespace Branding
{
    inline const char *productName() { return "terraPen"; }
    inline const char *siteLabel() { return "terrapen.xyz"; }

    // The panel's own mDNS name, announced by wifi_manager and printed on
    // the About screen. Here rather than as a literal in both, because the
    // two drifting apart means About confidently tells you an address that
    // doesn't answer. Distinct from FluidNC's own "terrapen" and
    // terraPixel's "terrapen-leds".
    inline const char *mdnsHostname() { return "terradial"; }
    inline const char *mdnsAddress() { return "terradial.local"; }

    inline const char *siteUrl() { return "https://terrapen.xyz"; }
    inline const char *githubUrl() { return "https://github.com/warderoid-ctrl/terraDial"; }

    // TODO: paste the real invite. Left empty deliberately -- an invented
    // invite code would render a QR that silently goes nowhere, which is
    // worse than not showing one at all.
    inline const char *discordUrl() { return ""; }
}
