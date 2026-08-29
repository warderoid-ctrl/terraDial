#include "panel_ring.h"
#include <Arduino.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>
#include "pins.h"

PanelRing panelRing;

static Adafruit_NeoPixel strip(LED_RING_COUNT, PIN_LED_RING, NEO_GRB + NEO_KHZ800);
static uint8_t sineTab[256];

// Same durations terraPixel uses for its "just finished a job" celebration.
static const uint32_t CELEBRATE_MS = 12000;
static const uint32_t FRAME_MS = 33; // ~30fps -- was 20 (50fps); this is decorative, not worth the extra strip.show() calls (each briefly disables interrupts to bit-bang WS2812 timing)

static uint8_t sin8t(uint8_t theta) { return sineTab[theta]; }

// Oscillate between lo and hi at the given bpm -- ported from terraPixel's beat().
static uint8_t beat(uint8_t bpm, uint8_t lo, uint8_t hi)
{
    uint32_t phase = (millis() * bpm * 256UL) / 60000UL;
    uint16_t v = sin8t(phase & 0xFF);
    return lo + ((uint32_t)v * (hi - lo)) / 255;
}

static void buildSineTable()
{
    for (int i = 0; i < 256; i++)
        sineTab[i] = (uint8_t)(127.5 + 127.4 * sin(i * 2.0 * PI / 256.0));
}

void PanelRing::begin()
{
    if (!sineTableBuilt_)
    {
        buildSineTable();
        sineTableBuilt_ = true;
    }
    strip.begin();
    strip.setBrightness((brightnessPct_ * 255) / 100);
    strip.clear();
    strip.show();
    modeEnteredAt_ = millis();
}

void PanelRing::setMode(MachineMode mode)
{
    if (mode == mode_) return;
    mode_ = mode;
    modeEnteredAt_ = millis();
}

void PanelRing::setBrightness(uint8_t percent)
{
    if (percent > 100) percent = 100;
    brightnessPct_ = percent;
}

void PanelRing::fillAll(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < LED_RING_COUNT; i++) strip.setPixelColor(i, r, g, b);
}

void PanelRing::render()
{
    uint32_t t = millis();
    uint8_t userBright = (brightnessPct_ * 255) / 100;

    switch (mode_)
    {
        case MachineMode::Run:
        {
            if (plotting_)
            {
                // A plot: the whole ring lit, breathing slowly, for however
                // long the job takes. The chase below is the right read for
                // a jog and the wrong one for an hour-long plot sitting in
                // your peripheral vision -- motion is what the eye keeps
                // going back to, and one pixel racing round a dark ring is
                // nearly all motion.
                //
                // Scaled INSIDE the user's brightness rather than driven at
                // absolute levels like the Hold/Alarm/Boot pulses. Two
                // reasons: the Display brightness setting should still mean
                // something during the longest thing the machine does, and
                // ScreenSleep dims this same value when the panel sleeps --
                // so the ring fades down with the screen and stays lit
                // through the night instead of blazing on at a fixed level.
                uint8_t lo = (uint8_t)(((uint32_t)userBright * 45) / 100);
                strip.setBrightness(beat(16, lo, userBright)); // ~3.75s per breath
                fillAll(255, 250, 240);
                break;
            }

            // No position feed on this ring (unlike terraPixel's rail comet) --
            // a single bright pixel chases around the 5 LEDs to read as "active".
            strip.setBrightness(userBright);
            fillAll(0, 0, 0);
            int step = (int)((t / 120) % LED_RING_COUNT);
            // Confirmed on hardware: this strip's pixel order runs
            // COUNTER-clockwise around the panel, so ascending index walks
            // the ring backwards relative to the knob. Hence clockwise
            // (chaseDir_ >= 0) is the descending-index case, not the
            // ascending one -- the obvious mapping had the light sweeping
            // opposite the direction the dial was turned.
            int idx = chaseDir_ >= 0 ? (LED_RING_COUNT - 1 - step) : step;
            strip.setPixelColor(idx, 255, 250, 240);
            break;
        }

        case MachineMode::Done:
            if (t - modeEnteredAt_ > CELEBRATE_MS) { mode_ = MachineMode::Idle; modeEnteredAt_ = t; break; }
            strip.setBrightness(beat(30, 90, 255));
            fillAll(0, 255, 60);
            break;

        case MachineMode::Homing:
        {
            strip.setBrightness(userBright);
            const uint32_t SWEEP_MS = 400, HOLD_MS = 120, GAP_MS = 120;
            const uint32_t CYCLE_MS = SWEEP_MS + HOLD_MS + GAP_MS;
            uint32_t phase = t % CYCLE_MS;
            int lit;
            if (phase < SWEEP_MS) lit = (int)((phase * (uint32_t)LED_RING_COUNT) / SWEEP_MS);
            else if (phase < SWEEP_MS + HOLD_MS) lit = LED_RING_COUNT;
            else lit = -1;
            for (int i = 0; i < LED_RING_COUNT; i++)
                strip.setPixelColor(i, i < lit ? 255 : 0, i < lit ? 90 : 0, 0);
            break;
        }

        case MachineMode::Hold:
            strip.setBrightness(beat(60, 60, 200));
            fillAll(255, 130, 0);
            break;

        case MachineMode::Alarm:
            strip.setBrightness(((t / 300) % 2) ? 200 : 20);
            fillAll(255, 0, 0);
            break;

        case MachineMode::Boot:
            strip.setBrightness(beat(20, 10, 50));
            fillAll(0, 40, 255);
            break;

        case MachineMode::Idle:
        default:
            strip.setBrightness(userBright);
            for (int i = 0; i < LED_RING_COUNT; i++)
            {
                uint8_t v = sin8t((i * 51) + (t / 24)); // 51 ~= 256/5, spread evenly
                strip.setPixelColor(i, 40 + (v >> 2), 22 + (v >> 3), 4);
            }
            break;
    }
}

void PanelRing::update()
{
    uint32_t now = millis();
    if (now - lastFrameAt_ < FRAME_MS) return;
    lastFrameAt_ = now;
    render();
    strip.show();
}
