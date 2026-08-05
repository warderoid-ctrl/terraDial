#pragma once

// Top-level radial navigation: knob rotate cycles screens, knob click drills
// in (stage-1: no-op, just here as the seam later stages hook into), long
// press always jumps back to Status.
namespace UiNav
{
    void begin();
    void update();

    // Navigates back to the Home dial -- same effect as the knob's
    // universal long-press-back convention. Exposed so any screen's
    // on-screen back-arrow button (see ui_screen_shell.h's addBackButton())
    // can trigger it directly.
    void goHome();
}
