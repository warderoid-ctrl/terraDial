#pragma once

// Physical travel limits of the plotter this panel is driving.
//
// Hard-coded for now, deliberately: every terraPen is currently the same
// size, and the alternative -- a Settings card with an X and a Y field --
// is a bigger job than the one thing that needs the numbers today (the
// Park-for-photo macro, display/ui_park.h).
//
// When that changes, this is the seam: move these two into AppSettings
// (config/settings.h) with these values as the NVS defaults, and every
// caller keeps working. Nothing else in the firmware knows the machine's
// size, so this header is the whole surface.
//
// COORDINATE CONVENTION: these are machine coordinates as FluidNC reports
// them after a successful $H -- i.e. this assumes the machine homes to its
// MINIMUM on each axis, leaving MPos 0,0 at the near-left corner and the
// far end of travel at a POSITIVE coordinate. A machine configured to home
// to max reports 0 at the far end and negative coordinates across the bed;
// on one of those, MACHINE_Y_MAX_MM would need to be negative.
// Only Y is defined, because only Y is known and only Y is used. An X
// extent would be a guess sitting in a header looking like a measurement.
static const float MACHINE_Y_MAX_MM = 420.0f;
