#include "rail_leds.h"
#include <Arduino.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>
#include "pins.h"
#include "../config/settings.h"
#include "../display/screen_sleep.h"

RailLeds railLeds;

static Adafruit_NeoPixel strip(RAIL_LED_COUNT, PIN_RAIL_LEDS, NEO_GRB + NEO_KHZ800);
static uint8_t sineTab[256];

static uint8_t sin8t(uint8_t theta) { return sineTab[theta]; }

// Oscillate between lo and hi at the given bpm -- terraPixel's beat().
static uint8_t beat(uint8_t bpm, uint8_t lo, uint8_t hi)
{
    uint32_t phase = (millis() * bpm * 256UL) / 60000UL;
    uint16_t v = sin8t(phase & 0xFF);
    return lo + ((uint32_t)v * (hi - lo)) / 255;
}

void RailLeds::begin()
{
    if (!sineTableBuilt_)
    {
        for (int i = 0; i < 256; i++)
            sineTab[i] = (uint8_t)(127.5 + 127.4 * sin(i * 2.0 * PI / 256.0));
        sineTableBuilt_ = true;
    }
    strip.begin();
    strip.setBrightness(BRIGHT_IDLE);
    strip.clear();
    strip.show();
}

void RailLeds::toggleParty()
{
    if (Config::get().railFilmMode) return; // never mid-take
    party_ = !party_;
}

void RailLeds::fillAll(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < RAIL_LED_COUNT; i++) strip.setPixelColor(i, r, g, b);
}

float RailLeds::xToLedIndex(float x) const
{
    x = constrain(x + POS_OFFSET_MM, 0.0f, X_TRAVEL_MM);
    x = X_TRAVEL_MM - x; // X=0 is the far end of the strip, not the near end
    float usableLeds = (RAIL_LED_COUNT - 1) - 2 * STRIP_MARGIN_LEDS;
    return STRIP_MARGIN_LEDS + (x / X_TRAVEL_MM) * usableLeds;
}

void RailLeds::renderComet()
{
    fillAll(0, 0, 0);
    if (!havePos_)
    {
        fillAll(255, 250, 240); // no position yet -- solid white beats a comet parked at a lie
        return;
    }

    float center = xToLedIndex(posX_); // deliberately not rounded -- keeps the fade continuous
    float radius = Config::get().railCometRadius;
    int lo = (int)floorf(center - radius);
    int hi = (int)ceilf(center + radius);
    for (int idx = lo; idx <= hi; idx++)
    {
        if (idx < 0 || idx >= RAIL_LED_COUNT) continue;
        float d = idx - center;
        if (fabsf(d) > radius) continue;
        // Raised-cosine taper: 1.0 at the exact position, 0.0 at the radius
        // edge, smooth between -- no LED ever pops from off to full bright.
        float b = 0.5f * (1.0f + cosf(PI * d / radius));
        uint8_t bright = (uint8_t)(b * 255);
        strip.setPixelColor(idx, (255UL * bright) / 255, (250UL * bright) / 255, (240UL * bright) / 255);
    }
}

void RailLeds::render()
{
    uint32_t t = millis();

    if (party_)
    {
        strip.setBrightness(BRIGHT_ALERT);
        for (int i = 0; i < RAIL_LED_COUNT; i++)
        {
            uint16_t h = rainbowHue_ + (i * 65536L / RAIL_LED_COUNT);
            strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(h)));
        }
        rainbowHue_ += 512;
        return;
    }

    switch (mode_)
    {
        case MachineMode::Run:
            strip.setBrightness(BRIGHT_WORK);
            renderComet(); // warm-white dot follows the carriage's X
            break;

        case MachineMode::Done:
            if (t - doneAt_ > CELEBRATE_MS) { mode_ = MachineMode::Idle; break; }
            strip.setBrightness(beat(30, 90, 255));
            fillAll(0, 255, 60); // green
            break;

        case MachineMode::Homing:
        {
            // Sequential fill down the strip, hold lit, blank, repeat -- like
            // a car's dynamic indicator rather than a wrapping dot-chase.
            strip.setBrightness(BRIGHT_ALERT);
            const uint32_t SWEEP_MS = 600, HOLD_MS = 150, GAP_MS = 150;
            const uint32_t CYCLE_MS = SWEEP_MS + HOLD_MS + GAP_MS;
            uint32_t phase = t % CYCLE_MS;
            int lit;
            if (phase < SWEEP_MS) lit = (int)((phase * (uint32_t)RAIL_LED_COUNT) / SWEEP_MS);
            else if (phase < SWEEP_MS + HOLD_MS) lit = RAIL_LED_COUNT;
            else lit = -1; // gap: all off
            for (int i = 0; i < RAIL_LED_COUNT; i++)
                strip.setPixelColor(i, i < lit ? 255 : 0, i < lit ? 90 : 0, 0); // indicator orange
            break;
        }

        case MachineMode::Hold:
            strip.setBrightness(beat(60, 60, BRIGHT_ALERT));
            fillAll(255, 130, 0); // amber
            break;

        case MachineMode::Alarm:
            strip.setBrightness(((t / 300) % 2) ? BRIGHT_ALERT : 20);
            fillAll(255, 0, 0);
            break;

        case MachineMode::Boot:
            strip.setBrightness(beat(20, 10, 50));
            fillAll(0, 40, 255); // dim blue breathing = not connected
            break;

        case MachineMode::Idle:
        default:
            strip.setBrightness(BRIGHT_IDLE);
            for (int i = 0; i < RAIL_LED_COUNT; i++) // slow warm shimmer
            {
                uint8_t v = sin8t((i * 6) + (t / 24));
                strip.setPixelColor(i, 40 + (v >> 2), 22 + (v >> 3), 4);
            }
            break;
    }

    // Film mode wants even, predictable exposure -- flatten whatever
    // brightness the mode above chose (including the beat()-driven pulsing)
    // in favour of one steady level.
    if (Config::get().railFilmMode) strip.setBrightness(Config::get().railFilmBrightness);
}

bool RailLeds::applySleepPolicy()
{
    if (!ScreenSleep::isAsleep()) return true;

    // The rail is task and status lighting for the MACHINE, not screen
    // backlight -- a long plot is exactly when you still want to see what
    // it's doing from across the room. So the default is to carry on
    // untouched while the panel sleeps; the other modes exist for when the
    // lights would be a nuisance (or ruin a timelapse's exposure).
    switch (Config::get().railSleepMode)
    {
        case RAIL_SLEEP_OFF:
            strip.clear();
            strip.show();
            return false;
        case RAIL_SLEEP_DIM:
            render();
            strip.setBrightness((uint8_t)((Config::get().railSleepBrightnessPct * 255) / 100));
            strip.show();
            return false; // already drawn at the dimmed level
        case RAIL_SLEEP_ON:
        default:
            return true;
    }
}

void RailLeds::update(MachineMode mode, float posX, bool havePos)
{
    if (mode != mode_)
    {
        if (mode == MachineMode::Done) doneAt_ = millis();
        mode_ = mode;
    }
    posX_ = posX;
    havePos_ = havePos;

    uint32_t now = millis();
    if (now - lastFrameAt_ < FRAME_MS) return;
    lastFrameAt_ = now;

    if (!applySleepPolicy()) return;

    render();
    strip.show();
}
