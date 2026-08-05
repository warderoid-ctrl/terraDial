#pragma once

#include <stdint.h>

// Shared machine-state vocabulary, mirrored 1:1 with terraPixel's Mode enum
// (warderoid-ctrl/terraPixel, src/main.cpp) so the panel's LED ring, the
// FluidNC status parser, and (later) the terraPixel remote all speak the
// same language.
enum class MachineMode : uint8_t
{
    Boot,  // not yet connected to FluidNC
    Idle,
    Run,
    Hold,
    Alarm,
    Homing,
    Done
};
