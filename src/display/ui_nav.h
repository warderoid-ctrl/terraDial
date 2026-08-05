#pragma once

// Top-level radial navigation: knob rotate cycles screens, knob click drills
// in (stage-1: no-op, just here as the seam later stages hook into), long
// press always jumps back to Status.
namespace UiNav
{
    void begin();
    void update();
}
