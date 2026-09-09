#pragma once

// Firmware version and the repo it updates from.
//
// FIRMWARE_VERSION is normally injected by tools/version.py at build time
// from the git tag (see platformio.ini). The fallback below only applies to
// a build with no tag reachable -- a working copy mid-development -- and is
// deliberately not a plausible version number: OTA refuses to "update" from
// a dev build to a release, since the dev build is very likely NEWER than
// whatever is published and clobbering it with an older release mid-test is
// the more annoying failure.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

namespace Version
{
    inline const char *firmware() { return FIRMWARE_VERSION; }

    // True for a build that didn't come from a tag. Version comparison is
    // meaningless for these.
    inline bool isDevBuild()
    {
        const char *v = FIRMWARE_VERSION;
        // A release version starts with a digit ("0.2.1"); "dev" and
        // "0.2.1-3-gabc123-dirty" (git describe on an untagged commit)
        // don't qualify -- the latter is caught by version.py, which only
        // emits a bare version for an exact tag.
        return !(v[0] >= '0' && v[0] <= '9');
    }
}
