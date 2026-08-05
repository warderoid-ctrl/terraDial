#include "encoder.h"
#include <Arduino.h>
#include "pins.h"

JogWheel jogWheel;

namespace
{
    // Interrupt-driven quadrature decode, standard 2-bit state-transition
    // table: index = (prevState<<2)|newState, each state is (A<<1)|B.
    // Invalid/bounce transitions (both bits appearing to change at once)
    // map to 0 and are ignored rather than guessed at.
    volatile int8_t qLastState = 0;
    volatile int32_t qRawTicks = 0;

    const int8_t QUAD_TABLE[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0
    };

    void IRAM_ATTR onEncoderChange()
    {
        uint8_t a = digitalRead(PIN_ENCODER_A);
        uint8_t b = digitalRead(PIN_ENCODER_B);
        uint8_t newState = (a << 1) | b;
        uint8_t idx = ((uint8_t)qLastState << 2) | newState;
        qRawTicks += QUAD_TABLE[idx & 0x0F];
        qLastState = newState;
    }

    // This EC11-style mechanical encoder latches at one detent per full
    // 4-tick quadrature cycle (the common case for these panel jog wheels).
    const int32_t TICKS_PER_DETENT = 4;
}

void JogWheel::begin()
{
    pinMode(PIN_ENCODER_A, INPUT);
    pinMode(PIN_ENCODER_B, INPUT);
    pinMode(PIN_ENCODER_SW, INPUT_PULLUP);

    qLastState = (digitalRead(PIN_ENCODER_A) << 1) | digitalRead(PIN_ENCODER_B);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), onEncoderChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), onEncoderChange, CHANGE);
}

void JogWheel::update()
{
    updateRotation();
    updateButton();
}

void JogWheel::updateRotation()
{
    noInterrupts();
    int32_t ticks = qRawTicks;
    interrupts();

    int32_t diff = ticks - lastRawTicks_;
    lastRawTicks_ = ticks;
    pendingTicks_ += diff;

    int32_t detents = pendingTicks_ / TICKS_PER_DETENT;
    pendingTicks_ -= detents * TICKS_PER_DETENT;
    rotationAccum_ += detents;

    if (detents != 0) lastActivityAt_ = millis();
}

void JogWheel::updateButton()
{
    uint32_t now = millis();
    bool rawLow = digitalRead(PIN_ENCODER_SW) == LOW;

    if (rawLow != rawLevelLow_)
    {
        rawLevelLow_ = rawLow;
        levelSince_ = now;
    }
    else if (now - levelSince_ >= DEBOUNCE_MS)
    {
        // Stable reading -- act on transitions into/out of "pressed".
        if (rawLow && !pressed_)
        {
            pressed_ = true;
            pressedAt_ = now;
            longPressFired_ = false;
            suppressPress_ = false;
            pressStartRawTicks_ = lastRawTicks_;
            lastActivityAt_ = now;
        }
        else if (!rawLow && pressed_)
        {
            pressed_ = false;
            if (!longPressFired_ && !suppressPress_)
            {
                pendingClicks_++;
                lastReleaseAt_ = now;
            }
        }
    }

    int32_t movedTicks = lastRawTicks_ - pressStartRawTicks_;
    if (movedTicks < 0) movedTicks = -movedTicks;
    if (pressed_ && !suppressPress_ && movedTicks > PRESS_MOVEMENT_SUPPRESS_TICKS)
    {
        suppressPress_ = true;
    }

    if (pressed_ && !suppressPress_ && !longPressFired_ && (now - pressedAt_ >= LONG_PRESS_MS))
    {
        longPressFired_ = true;
        pendingClicks_ = 0; // long press pre-empts any pending click/double-click
        pendingEvent_ = ButtonEvent::LongPress;
    }

    if (pendingClicks_ > 0 && !pressed_ && (now - lastReleaseAt_ > DOUBLE_CLICK_MS))
    {
        pendingEvent_ = (pendingClicks_ >= 2) ? ButtonEvent::DoubleClick : ButtonEvent::Click;
        pendingClicks_ = 0;
    }
}

int32_t JogWheel::takeRotationDelta()
{
    int32_t v = rotationAccum_;
    rotationAccum_ = 0;
    return v;
}

ButtonEvent JogWheel::takeButtonEvent()
{
    ButtonEvent e = pendingEvent_;
    pendingEvent_ = ButtonEvent::None;
    return e;
}
