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
    {100.0f, 3000.0f},
};
static const int JOG_STEP_COUNT = 4;

// Largest step offered on Z. Z is the pen lift -- a few mm of travel between
// the bed and the top stop -- so the X/Y steps above this would drive the pen
// into one or the other.
//
// Deliberately a DISTANCE, not an index. This was "hide the last chip in the
// table", which silently meant "10mm" only for as long as 10mm happened to be
// last: adding the 100mm step below it would have quietly promoted 10mm to a
// legal Z move without a line of this file appearing to change.
static const float JOG_MAX_Z_MM = 1.0f;

// Pen up/down: fixed 5mm relative Z jog -- not one of the JOG_STEPS
// presets above, just a flat button (no step-size selector).
static const float PEN_JOG_MM = 5.0f;
static const float PEN_JOG_FEED = 1500.0f; // mm/min
