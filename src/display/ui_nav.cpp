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
#include "../net/fluidnc_client.h"

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

    void goTo(int index, lv_scr_load_anim_t anim)
    {
        if (currentIndex == index) return;
        currentIndex = index;
        lv_scr_load_anim(screens[currentIndex], anim, 180, 0, false);

        if (currentIndex == FILES_SCREEN_INDEX) uiFilesSetFocused(true);
        if (currentIndex == LIGHTS_SCREEN_INDEX) uiLightsOnShow();
    }

    // Maps a Home carousel card index to what opening it does. Order
    // matches ui_dial.cpp's DIAL_ITEMS: Jobs, Jog, Pen, Lights, Home,
    // E-Stop, Alarm, Settings.
    void onDialOpen(int cardIndex)
    {
        switch (cardIndex)
        {
            case 0: goTo(FILES_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 1: goTo(JOG_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 2: goTo(PEN_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 3: goTo(LIGHTS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 4: goTo(HOME_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 5: goTo(ESTOP_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 6: goTo(ALARM_CLEAR_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 7: goTo(SETTINGS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
        }
    }

    // Auto-navigation on machine-state transitions, per the mockup's
    // "intended to appear automatically" (Alarm Clear) and Job Progress
    // being reached by starting a job rather than as a direct dial card.
    void checkAutoNav()
    {
        static MachineMode lastMode = MachineMode::Boot;
        MachineMode mode = fluidNC.status().mode;
        if (mode == lastMode) return;

        if (mode == MachineMode::Alarm)
        {
            goTo(ALARM_CLEAR_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
        }
        else if (mode == MachineMode::Run && lastMode != MachineMode::Run)
        {
            goTo(JOB_PROGRESS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
        }
        else if ((lastMode == MachineMode::Run || lastMode == MachineMode::Hold) &&
                 (mode == MachineMode::Idle || mode == MachineMode::Done))
        {
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

        int32_t delta = jogWheel.takeRotationDelta();
        ButtonEvent ev = jogWheel.takeButtonEvent();

        if (currentIndex == DIAL_SCREEN_INDEX)
        {
            // delta can be more than +-1 if several detents land in one
            // loop iteration -- step the highlight once per detent instead
            // of once per call, or the extras get silently dropped (felt
            // as needing several physical clicks before anything visibly
            // moves).
            for (int32_t i = 0; i < delta; i++) uiDialSelectNext();
            for (int32_t i = 0; i < -delta; i++) uiDialSelectPrev();

            if (ev == ButtonEvent::Click) uiDialOpenSelected();
            else if (ev == ButtonEvent::LongPress) goTo(SETTINGS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
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
            if (delta != 0) uiFilesHandleRotate(delta);
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
            if (delta != 0) uiSettingsHandleRotate(delta);
            if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
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

        // Home($H)/Lights: touch-only content -- the knob's only role here
        // is the universal "long press = back to the dial".
        if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
    }
}
