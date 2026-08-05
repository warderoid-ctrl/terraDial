#pragma once

#include <stdint.h>
#include <Arduino.h> // millis()

enum class ButtonEvent : uint8_t
{
    None,
    Click,
    DoubleClick,
    LongPress
};

// Driver for the CrowPanel jog wheel (quadrature encoder + integrated push
// button).
//
// Rotation is decoded by a hardware interrupt on BOTH encoder pins using a
// standard 2-bit quadrature state-transition table, not by polling one pin
// from the main loop. Polling only catches transitions between loop
// iterations; at high spin speeds the mechanical detents arrive faster than
// the loop polls them, so a poll-based read silently misreads direction on
// some transitions (confirmed on hardware: fast spins would move correctly
// then randomly jump the wrong way). The ISR can't be outrun this way since
// it fires on every real edge regardless of what the main loop is doing;
// takeRotationDelta() just reports the net whole detents accumulated since
// the last call, so a fast spin between two polls naturally reports a larger
// delta than a slow one -- no separate speed heuristic needed.
//
// The button is still polled/debounced in software (not ISR-driven): the
// terraPixel firmware's own notes call out that stepper-driver EMI causes
// edge noise on nearby GPIOs, so it requires a stable reading held for a
// debounce window before counting as a real press.
class JogWheel
{
public:
    void begin();

    // Call frequently (every main-loop iteration, ~5-10ms). Non-blocking.
    void update();

    // Net detent count accumulated since the last call (reset on read).
    // Positive = clockwise, negative = counter-clockwise.
    int32_t takeRotationDelta();

    ButtonEvent takeButtonEvent();

    // Milliseconds since the last rotation or button state change -- used
    // for the backlight idle timeout. Doesn't consume/reset anything (safe
    // to call as often as needed, independent of takeRotationDelta()).
    uint32_t msSinceActivity() const { return millis() - lastActivityAt_; }

private:
    int32_t lastRawTicks_ = 0;
    int32_t pendingTicks_ = 0;
    int32_t rotationAccum_ = 0;
    uint32_t lastActivityAt_ = 0;

    // Button state
    static const uint32_t DEBOUNCE_MS = 20;
    static const uint32_t LONG_PRESS_MS = 600;
    static const uint32_t DOUBLE_CLICK_MS = 300;

    bool rawLevelLow_ = false;
    uint32_t levelSince_ = 0;
    bool pressed_ = false;
    uint32_t pressedAt_ = 0;
    bool longPressFired_ = false;
    uint8_t pendingClicks_ = 0;
    uint32_t lastReleaseAt_ = 0;

    ButtonEvent pendingEvent_ = ButtonEvent::None;

    void updateRotation();
    void updateButton();
};

extern JogWheel jogWheel;
