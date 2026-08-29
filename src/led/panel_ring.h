#pragma once

#include <stdint.h>
#include "machine_mode.h"

// This module is deliberately self-contained -- it just renders whatever
// mode it's told; net/fluidnc_client.cpp calls setMode() from the parsed
// FluidNC status stream.

class PanelRing
{
public:
    void begin();
    void setMode(MachineMode mode);

    // Manual brightness override, 0-100. Ignored while a mode's own pulsing
    // brightness is driving the show (Hold/Alarm/Boot/Done) -- those need
    // their brightness swing to stay legible.
    void setBrightness(uint8_t percent);
    uint8_t brightness() const { return brightnessPct_; }

    // Which way the jog chase animation travels: +1 clockwise, -1
    // counter-clockwise. Set from the Jog screen so the light visibly
    // follows the knob -- turning the dial one way and watching the ring
    // sweep the other reads as the machine doing the opposite of what you
    // asked.
    void setChaseDirection(int8_t dir) { chaseDir_ = dir >= 0 ? 1 : -1; }

    // Whether the Run the ring is showing is a PLOT rather than a jog.
    //
    // FluidNC reports both as Run, but they want opposite treatments and the
    // ring can't tell them apart on its own. A jog is a thing you are doing
    // with your hand right now, so it gets the chase, which follows the knob
    // and answers "did that go the way I meant". A plot is a thing you are
    // living beside for an hour, so it gets a slow breath: still obviously
    // alive from across the room, but not a white dot circling in the corner
    // of your eye the whole time.
    void setPlotting(bool plotting) { plotting_ = plotting; }

    // Call every loop iteration; internally time-gated to ~50fps.
    void update();

private:
    MachineMode mode_ = MachineMode::Boot;
    uint8_t brightnessPct_ = 60;
    uint32_t modeEnteredAt_ = 0;
    uint32_t lastFrameAt_ = 0;
    uint16_t rainbowHue_ = 0;
    int8_t chaseDir_ = 1;
    bool plotting_ = false;
    bool sineTableBuilt_ = false;

    void render();
    void fillAll(uint8_t r, uint8_t g, uint8_t b);
};

extern PanelRing panelRing;
