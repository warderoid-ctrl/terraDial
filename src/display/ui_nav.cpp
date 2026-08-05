#include "ui_nav.h"
#include <lvgl.h>
#include <string.h>
#include "ui_dial.h"
#include "ui_jog.h"
#include "ui_files.h"
#include "ui_pen.h"
#include "ui_home.h"
#include "ui_lights.h"
#include "ui_settings.h"
#include "../input/encoder.h"
#include "../net/fluidnc_client.h"

namespace
{
    const int SCREEN_COUNT = 7;
    const int DIAL_SCREEN_INDEX = 0;
    const int JOG_SCREEN_INDEX = 1;
    const int FILES_SCREEN_INDEX = 2;
    const int PEN_SCREEN_INDEX = 3;
    const int HOME_SCREEN_INDEX = 4;
    const int LIGHTS_SCREEN_INDEX = 5;
    const int SETTINGS_SCREEN_INDEX = 6;

    lv_obj_t *screens[SCREEN_COUNT];
    int currentIndex = DIAL_SCREEN_INDEX;

    void goTo(int index, lv_scr_load_anim_t anim)
    {
        currentIndex = index;
        lv_scr_load_anim(screens[currentIndex], anim, 180, 0, false);

        // Arriving at these screens via the dial IS "opening" them (per the
        // dial's own tap-to-select/tap-again-to-open model) -- no separate
        // in-screen "focus" step needed anymore, so activate their
        // knob-driven behavior and (for Files) kick off a fresh listing
        // immediately.
        if (currentIndex == JOG_SCREEN_INDEX) uiJogSetFocused(true);
        if (currentIndex == FILES_SCREEN_INDEX) uiFilesSetFocused(true);
        if (currentIndex == LIGHTS_SCREEN_INDEX) uiLightsOnShow();
    }

    // Maps a dial wedge index to what opening it does. Order matches
    // ui_dial.cpp's DIAL_ITEMS: Jog, Jobs, Pen, Lights, Home, Stop.
    void onDialOpen(int wedgeIndex)
    {
        switch (wedgeIndex)
        {
            case 0: goTo(JOG_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 1: goTo(FILES_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 2: goTo(PEN_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 3: goTo(LIGHTS_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 4: goTo(HOME_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON); break;
            case 5: fluidNC.feedHold(); break; // Stop: immediate action, no screen to open
        }
    }

    void alarmClearConfirmCb(lv_event_t *e)
    {
        lv_obj_t *mbox = lv_event_get_current_target(e);
        const char *txt = lv_msgbox_get_active_btn_text(mbox);
        if (txt && !strcmp(txt, "Clear")) fluidNC.clearAlarm();
        lv_msgbox_close(mbox);
    }

    void onDialBack()
    {
        // Tapping the hub while already on the dial: if the machine is
        // alarmed, offer to clear it ($X) -- otherwise nothing to back out
        // of yet; reserved (e.g. for canceling an armed-but-unopened wedge
        // back to a neutral selection).
        if (fluidNC.status().mode == MachineMode::Alarm)
        {
            static const char *btns[] = {"Clear", "Cancel", ""};
            lv_obj_t *mbox = lv_msgbox_create(NULL, "Alarm", "Clear alarm state ($X)?", btns, false);
            lv_obj_center(mbox);
            lv_obj_add_event_cb(mbox, alarmClearConfirmCb, LV_EVENT_VALUE_CHANGED, NULL);
        }
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

        uiDialSetHandlers(onDialOpen, onDialBack);

        lv_scr_load(screens[DIAL_SCREEN_INDEX]);
    }

    void update()
    {
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
            if (ev == ButtonEvent::Click) uiJogCycleStep();
            else if (ev == ButtonEvent::DoubleClick) uiJogCycleAxis();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        if (currentIndex == FILES_SCREEN_INDEX)
        {
            if (delta != 0) uiFilesHandleRotate(delta);
            if (ev == ButtonEvent::Click) uiFilesHandleSelect();
            else if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
            return;
        }

        // Pen/Home/Lights/Settings: touch-only content -- the knob's only
        // role here is the universal "long press = back to the dial".
        if (ev == ButtonEvent::LongPress) goTo(DIAL_SCREEN_INDEX, LV_SCR_LOAD_ANIM_FADE_ON);
    }
}
