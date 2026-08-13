#pragma once
#include <stdbool.h>

// Screen sleep: after a configurable idle period the backlight goes fully
// off and the LED ring drops to a separate "asleep" brightness, so the panel
// isn't glowing at you through a multi-hour plot while the ring still shows
// machine state across the room.
//
// Waking is deliberately inert. The touch or knob input that wakes the panel
// is SWALLOWED -- it must never jog an axis, open a menu or start a job.
// Grabbing a dark panel to see what's happening is exactly the moment you
// can least afford a stray command to reach the plotter.
//
// Sleeping is never blocked by machine state: a long job is the main reason
// this exists, so it sleeps right through one.
namespace ScreenSleep
{
    void begin();

    // Call every loop iteration -- puts the panel to sleep once idle.
    void update();

    bool isAsleep();

    // Call on ANY real user input. Resets the idle timer, and returns true
    // if that input arrived while asleep -- in which case the caller must
    // discard it rather than acting on it.
    bool noteInputAndWake();
}
