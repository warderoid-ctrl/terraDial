# PlatformIO pre-build script: stamp the firmware with a version string.
#
# Priority:
#   1. $TERRADIAL_VERSION      -- what CI sets, from the pushed tag
#   2. `git describe --tags --exact-match` on HEAD -- a local tagged build
#   3. "dev"                   -- anything else
#
# Only an EXACT tag yields a release version. A commit that merely descends
# from a tag is a development build, and stamping it "0.2.0" would make an
# OTA check compare a newer local build against an older published release
# and offer to downgrade it. See include/version.h.
import subprocess, os

Import("env")  # noqa: F821  (injected by PlatformIO)


def _git(*args):
    try:
        return subprocess.check_output(
            ["git"] + list(args), stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return ""


version = os.environ.get("TERRADIAL_VERSION", "").strip()
if not version:
    version = _git("describe", "--tags", "--exact-match")

version = version.lstrip("v") if version else "dev"

print("terraDial firmware version: %s" % version)
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])
