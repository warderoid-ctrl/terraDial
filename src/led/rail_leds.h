#pragma once

#include <stdint.h>
#include "machine_mode.h"

// The terraPen's rail light strip: a warm-white comet that follows the
// carriage while plotting, plus per-mode status animations.
//
// Ported from terraPixel (warderoid-ctrl/terraPixel), which used to run this
// on a separate ESP32-C3 and take orders over HTTP. The panel mounts on the
// plotter beside the rail, so it drives the strip directly now -- one less
// board, and the comet reads the carriage position straight off the status
// stream this firmware already receives instead of terraPixel maintaining a
// second connection to FluidNC for it.
//
// The animations, timings and colours are carried over deliberately
// unchanged so the machine still "reads" the same across the room.
class RailLeds
{
public:
    void begin();

    // Call every loop iteration; internally frame-gated to ~50fps.
    // posX is the work-coordinate X in mm; havePos false falls back to a
    // solid strip rather than parking the comet at a lie.
    void update(MachineMode mode, float posX, bool havePos);

    // Rainbow chase. Suppressed entirely in film mode -- nobody wants to
    // discover a rainbow halfway through a take.
    void toggleParty();
    bool party() const { return party_; }

private:
    // ---- physical calibration ----
    // Strip is 39 LEDs at 16mm pitch = 608mm, mounted along a 660mm rail,
    // tracking 0..600mm of X travel. STRIP_MARGIN_LEDS keeps the comet off
    // the very ends; POS_OFFSET_MM is pure calibration -- if the lit LEDs sit
    // consistently to one side of the real tool head, nudge it (either sign)
    // until centred.
    static constexpr float X_TRAVEL_MM = 600.0f;
    static constexpr float POS_OFFSET_MM = 20.0f;
    static constexpr int STRIP_MARGIN_LEDS = 2;

    static const uint8_t BRIGHT_WORK = 255;
    static const uint8_t BRIGHT_IDLE = 60;
    static const uint8_t BRIGHT_ALERT = 200;

    static const uint32_t CELEBRATE_MS = 12000;
    static const uint32_t FRAME_MS = 20; // 50fps

    MachineMode mode_ = MachineMode::Boot;
    float posX_ = 0;
    bool havePos_ = false;

    bool party_ = false;
    uint32_t doneAt_ = 0;
    uint32_t lastFrameAt_ = 0;
    uint16_t rainbowHue_ = 0;
    bool sineTableBuilt_ = false;

    void fillAll(uint8_t r, uint8_t g, uint8_t b);
    float xToLedIndex(float x) const;
    void renderComet();
    void render();
    // Returns false if sleep policy says the strip should be dark this frame.
    bool applySleepPolicy();
};

extern RailLeds railLeds;
