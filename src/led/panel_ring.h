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

    // Which way the Run/Jog chase animation travels: +1 clockwise, -1
    // counter-clockwise. Set from the Jog screen so the light visibly
    // follows the knob -- turning the dial one way and watching the ring
    // sweep the other reads as the machine doing the opposite of what you
    // asked.
    void setChaseDirection(int8_t dir) { chaseDir_ = dir >= 0 ? 1 : -1; }

    // Call every loop iteration; internally time-gated to ~50fps.
    void update();

private:
    MachineMode mode_ = MachineMode::Boot;
    uint8_t brightnessPct_ = 60;
    uint32_t modeEnteredAt_ = 0;
    uint32_t lastFrameAt_ = 0;
    uint16_t rainbowHue_ = 0;
    int8_t chaseDir_ = 1;
    bool sineTableBuilt_ = false;

    void render();
    void fillAll(uint8_t r, uint8_t g, uint8_t b);
};

extern PanelRing panelRing;
