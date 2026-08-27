#include "ui_nav.h"
#include <lvgl.h>
#include "ui_dial.h"
#include "ui_jog.h"
#include "ui_files.h"
#include "ui_pen.h"
#include "ui_home.h"
#include "ui_lights.h"
#include "ui_settings.h"
#include "ui_job_progress.h"
#include "ui_estop.h"
#include "ui_alarm_clear.h"
#include "../input/encoder.h"
#include "radial_keyboard.h"
#include "screen_sleep.h"
#include "../net/fluidnc_client.h"
#include "../config/settings.h"

namespace
{
    const int SCREEN_COUNT = 10;
    const int DIAL_SCREEN_INDEX = 0;
    const int JOG_SCREEN_INDEX = 1;
    const int FILES_SCREEN_INDEX = 2;
    const int PEN_SCREEN_INDEX = 3;
    const int HOME_SCREEN_INDEX = 4;
    const int LIGHTS_SCREEN_INDEX = 5;
    const int SETTINGS_SCREEN_INDEX = 6;
    const int JOB_PROGRESS_SCREEN_INDEX = 7;
    const int ESTOP_SCREEN_INDEX = 8;
    const int ALARM_CLEAR_SCREEN_INDEX = 9;

    lv_obj_t *screens[SCREEN_COUNT];
    int currentIndex = DIAL_SCREEN_INDEX;

    // Set by checkAutoNav() when a real SD job auto-navigates to Job
    // Progress; cleared either when the job naturally finishes (see
    // checkAutoNav) or, here, whenever the user manually leaves Job
    // Progress by any other path (e.g. knob long-press back) while it's
    // still true. Without this, backing out of Job Progress mid-job left
    // the flag stuck -- the next unrelated Run->Idle blip from ANYWHERE
    // (e.g. a single pen-up/down jog) then satisfied checkAutoNav's
    // leaving-Run condition and yanked the screen back to Dial before the
    // user could see anything happen, which is what a stuck jog/pen
    // screen "doing nothing" actually was.
    bool jobFlowActive = false;

    void goTo(int index, lv_scr_load_anim_t anim)
    {
        if (currentIndex == index) return;
        if (currentIndex == JOB_PROGRESS_SCREEN_INDEX) jobFlowActive = false;
        currentIndex = index;
        lv_scr_load_anim(screens[currentIndex], anim, 120, 0, false); // was 180 -- snappier screen switching

        if (currentIndex == FILES_SCREEN_INDEX) uiFilesSetFocused(true);
        if (currentIndex == LIGHTS_SCREEN_INDEX) uiLightsOnShow();

        // Walking into Progress on a live job re-arms the flag that the
        // walk *out* cleared, so the job still hands the screen back to the
        // dial when it finishes. Otherwise a mid-run detour to the Lights
        // page permanently disarmed the end-of-job return.
        if (currentIndex == JOB_PROGRESS_SCREEN_INDEX && fluidNC.status().jobActive)
            jobFlowActive = true;
    }

    // Maps a Home ring item index to what opening it does. Order must
    // match ui_dial.cpp's DIAL_ITEMS: Home, Jog, Pen, Jobs, Progress,
    // E-Stop, Alarm, Lights, Settings.
    void onDialOpen(int cardIndex)
    {
        switch (cardIndex)
        {
            case 0: goTo(HOME_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 1: goTo(JOG_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 2: goTo(PEN_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 3: goTo(FILES_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 4: goTo(JOB_PROGRESS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 5: goTo(ESTOP_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 6: goTo(ALARM_CLEAR_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 7: goTo(LIGHTS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 8: goTo(SETTINGS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
        }
    }

    // Auto-navigation on machine-state transitions, per the mockup's
    // "intended to appear automatically" (Alarm Clear) and Job Progress
    // being reached by starting a job rather than as a direct dial card.
    //
    // Gated on FluidNCStatus::jobActive (jobFlowActive latch above, not
    // re-read from status() on exit -- see its header comment), not just
    // mode==Run: FluidNC reports "Jog" as a Run state too, so a bare mode
    // check fired this on every single knob-jog tick, yanking the screen
    // to Job Progress and immediately back to Dial regardless of what
    // screen the user was actually on.
    void checkAutoNav()
    {
        static MachineMode lastMode = MachineMode::Boot;
        static bool lastJobActive = false;

        MachineMode mode = fluidNC.status().mode;
        bool jobActive = fluidNC.status().jobActive;

        // A job we didn't start is recognised from the status report's SD:
        // field, which can arrive a report or two after the state flips to
        // Run. Watching only the mode transition would miss those: by the
        // time we knew it was a job, lastMode was already Run and the edge
        // had passed. So a job appearing counts as an edge of its own.
        bool modeChanged = (mode != lastMode);
        bool jobStarted = (jobActive && !lastJobActive);
        lastJobActive = jobActive;
        if (!modeChanged && !jobStarted) return;

        if (mode == MachineMode::Alarm && modeChanged)
        {
            jobFlowActive = false;
            goTo(ALARM_CLEAR_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
        }
        else if (mode == MachineMode::Run && jobActive &&
                 (lastMode != MachineMode::Run || jobStarted))
        {
            jobFlowActive = true;
            goTo(JOB_PROGRESS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
        }
        else if (jobFlowActive &&
                 (lastMode == MachineMode::Run || lastMode == MachineMode::Hold) &&
                 (mode == MachineMode::Idle || mode == MachineMode::Done))
        {
            jobFlowActive = false;
            goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
        }
        lastMode = mode;
    }
}

namespace UiNav
{
    void begin()
    {
        screens[DIAL_SCREEN_INDEX] = uiDialCreate();
        screens[JOG_SCREEN_INDEX] = uiJogCreate();
        screens[FILES_SCREEN_INDEX] = uiFilesCreate();
        screens[PEN_SCREEN_INDEX] = uiPenCreate();
        screens[HOME_SCREEN_INDEX] = uiHomeCreate();
        screens[LIGHTS_SCREEN_INDEX] = uiLightsCreate();
        screens[SETTINGS_SCREEN_INDEX] = uiSettingsCreate();
        screens[JOB_PROGRESS_SCREEN_INDEX] = uiJobProgressCreate();
        screens[ESTOP_SCREEN_INDEX] = uiEstopCreate();
        screens[ALARM_CLEAR_SCREEN_INDEX] = uiAlarmClearCreate();

        uiDialSetHandlers(onDialOpen);

        lv_scr_load(screens[DIAL_SCREEN_INDEX]);
    }

    void update()
    {
        checkAutoNav();

        // Only meaningful while Lights is visible. It's a cheap cache read
        // now that terraPixel's HTTP lives on the network task -- it used to
        // block here for ~1s every 3 seconds on EVERY screen, worst of all
        // when the controller wasn't on the network at all.
        if (currentIndex == LIGHTS_SCREEN_INDEX) uiLightsUpdate();

        // Same idea: only the alarm screen cares, and only while it's up.
        if (currentIndex == ALARM_CLEAR_SCREEN_INDEX) uiAlarmClearUpdate();

        int32_t delta = jogWheel.takeRotationDelta();
        ButtonEvent ev = jogWheel.takeButtonEvent();

        // Waking the panel must not do anything else. Both events were
        // already consumed by the take*() calls above, so returning here
        // discards them -- the turn or click that lit the screen can't also
        // jog an axis or open a menu.
        if (delta != 0 || ev != ButtonEvent::None)
        {
            if (ScreenSleep::noteInputAndWake()) return;
        }

        // The radial keyboard is modal and owns the knob outright while it's
        // up: rotate moves the highlighted key, click types it, long-press
        // cancels. Nothing below runs, so no screen can navigate out from
        // under an open text field.
        if (RadialKeyboard::isOpen())
        {
            if (delta != 0) RadialKeyboard::handleRotate(delta);
            if (ev == ButtonEvent::Click) RadialKeyboard::handleClick();
            else if (ev == ButtonEvent::LongPress) RadialKeyboard::handleLongPress();
            return;
        }

        // Menu-navigation direction preference (Settings > Display). Applied
        // here so every browsing surface (dial, Jobs, Settings) agrees --
        // but deliberately NOT to the Jog screen below, which passes the raw
        // delta straight through: jog direction maps to real machine motion,
        // so flipping it to suit a menu preference would be a safety trap.
        int32_t menuDelta = Config::get().invertMenuRotation ? -delta : delta;

        if (currentIndex == DIAL_SCREEN_INDEX)
        {
            // delta can be more than +-1 if several detents land in one
            // loop iteration -- step the highlight once per detent instead
            // of once per call, or the extras get silently dropped (felt
            // as needing several physical clicks before anything visibly
            // moves).
            for (int32_t i = 0; i < menuDelta; i++) uiDialSelectNext();
            for (int32_t i = 0; i < -menuDelta; i++) uiDialSelectPrev();

            // No LongPress shortcut here anymore -- it used to jump straight
            // to Settings, which read as a stray/unexplained screen change
            // whenever a click was held a little too long (Settings is a
            // normal dial destination like any other; it doesn't need a
            // shortcut, and the surprise wasn't worth it).
            if (ev == ButtonEvent::Click) uiDialOpenSelected();
            return;
        }

        if (currentIndex == JOG_SCREEN_INDEX)
        {
            if (delta != 0) uiJogHandleRotate(delta);
            if (ev == ButtonEvent::Click) uiJogCycleAxis();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == FILES_SCREEN_INDEX)
        {
            if (menuDelta != 0) uiFilesHandleRotate(menuDelta);
            if (ev == ButtonEvent::Click) uiFilesHandleSelect();
            else if (ev == ButtonEvent::DoubleClick) uiFilesHandleDoubleClick();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == PEN_SCREEN_INDEX)
        {
            if (ev == ButtonEvent::Click) uiPenToggle();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == SETTINGS_SCREEN_INDEX)
        {
            if (menuDelta != 0) uiSettingsHandleRotate(menuDelta);
            if (ev == ButtonEvent::Click) uiSettingsHandleClick();
            // Settings is two levels deep (category ring -> that category's
            // controls), so back steps out of an open category first and
            // only leaves the screen once the ring is showing.
            else if (ev == ButtonEvent::LongPress && !uiSettingsHandleBack())
                goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == JOB_PROGRESS_SCREEN_INDEX)
        {
            if (ev == ButtonEvent::Click) uiJobProgressTogglePause();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == ESTOP_SCREEN_INDEX)
        {
            if (ev == ButtonEvent::Click) uiEstopTrigger();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == ALARM_CLEAR_SCREEN_INDEX)
        {
            if (ev == ButtonEvent::Click) uiAlarmClearTrigger();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        // Home is a confirm screen like E-Stop/Alarm Clear -- knob click
        // confirms it, per the mockup's "knob click confirms Home/Alarm-clear
        // prompts".
        if (currentIndex == HOME_SCREEN_INDEX)
        {
            if (ev == ButtonEvent::Click) uiHomeTrigger();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == LIGHTS_SCREEN_INDEX)
        {
            // The page is taller than the round-safe area, so the knob
            // scrolls it -- matching how it scrolls an open Settings
            // category rather than leaving the knob doing nothing here.
            if (menuDelta != 0) uiLightsHandleRotate(menuDelta);
            if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        // Defensive default -- every screen above handles itself, but a new
        // one added without a case still gets the universal back gesture.
        if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
    }

    // Lets any screen's visible on-screen back arrow (ui_screen_shell.cpp's
    // addBackButton()) do the same thing the knob long-press already does,
    // for anyone who finds a long-press less discoverable than a button.
    void goHome()
    {
        goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
    }
}
