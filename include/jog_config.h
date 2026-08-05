#pragma once

// Shared jog step/feed presets -- used by both the Jog screen (X/Y/Z, knob
// rotate) and the Pen screen's Z-lift button, so a "step" moves identically
// wherever it's triggered from. All motion is via steppers.
struct JogStep
{
    float mm;
    float feedMmMin;
};

static const JogStep JOG_STEPS[] = {
    {0.1f, 200.0f},
    {1.0f, 800.0f},
    {10.0f, 2000.0f},
};
static const int JOG_STEP_COUNT = 3;

// Pen up/down: fixed 5mm relative Z jog -- not one of the JOG_STEPS
// presets above, just a flat button (no step-size selector).
static const float PEN_JOG_MM = 5.0f;
static const float PEN_JOG_FEED = 1500.0f; // mm/min
