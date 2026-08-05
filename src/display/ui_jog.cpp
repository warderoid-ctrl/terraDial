#include "ui_jog.h"
#include "jog_config.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <stdio.h>

namespace
{
    const char AXES[] = {'X', 'Y', 'Z'};
    const int AXIS_COUNT = 3;

    int stepIndex = 1; // default 1.0mm
    int axisIndex = 0; // default X

    lv_obj_t *focusRing = nullptr;
    lv_obj_t *axisLabel = nullptr;
    lv_obj_t *stepLabel = nullptr;
    lv_obj_t *posLabel = nullptr;

    void updateLabels()
    {
        char axisBuf[4];
        snprintf(axisBuf, sizeof(axisBuf), "%c", AXES[axisIndex]);
        lv_label_set_text(axisLabel, axisBuf);

        char stepBuf[24];
        snprintf(stepBuf, sizeof(stepBuf), "step %.1fmm", JOG_STEPS[stepIndex].mm);
        lv_label_set_text(stepLabel, stepBuf);
    }

    void stopBtnCb(lv_event_t *e)
    {
        (void)e;
        fluidNC.feedHold();
    }
}

lv_obj_t *uiJogCreate()
{
    ScreenShell shell = createScreenShell("JOG", LV_SYMBOL_GPS);
    focusRing = shell.ring;

    axisLabel = lv_label_create(shell.content);
    lv_label_set_text(axisLabel, "X");
    lv_obj_set_style_text_font(axisLabel, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(axisLabel, Palette::accent(), 0);

    stepLabel = lv_label_create(shell.content);
    lv_label_set_text(stepLabel, "step 1.0mm");
    lv_obj_set_style_text_font(stepLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(stepLabel, Palette::textMuted(), 0);

    posLabel = lv_label_create(shell.content);
    lv_label_set_text(posLabel, "--");
    lv_obj_set_style_text_font(posLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(posLabel, Palette::border(), 0);

    lv_obj_t *stopBtn = lv_btn_create(shell.content);
    lv_obj_set_size(stopBtn, 100, 44);
    lv_obj_set_style_bg_color(stopBtn, Palette::accent(), 0);
    lv_obj_set_style_radius(stopBtn, 10, 0);
    lv_obj_add_event_cb(stopBtn, stopBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stopLbl = lv_label_create(stopBtn);
    lv_label_set_text(stopLbl, "STOP");
    lv_obj_center(stopLbl);

    updateLabels();
    return shell.screen;
}

void uiJogSetFocused(bool focused)
{
    if (!focusRing) return;
    lv_obj_set_style_arc_color(focusRing, focused ? Palette::accent() : Palette::border(), LV_PART_INDICATOR);
}

void uiJogHandleRotate(int32_t delta)
{
    if (delta == 0) return;
    // delta carries direction and the encoder's own fast-turn weighting
    // (see input/encoder.cpp) -- used directly as a step multiplier so a
    // quick flick moves further per tick than a slow turn.
    float distanceMm = JOG_STEPS[stepIndex].mm * delta;
    fluidNC.jog(AXES[axisIndex], distanceMm, JOG_STEPS[stepIndex].feedMmMin);
}

void uiJogCycleStep()
{
    stepIndex = (stepIndex + 1) % JOG_STEP_COUNT;
    updateLabels();
}

void uiJogCycleAxis()
{
    axisIndex = (axisIndex + 1) % AXIS_COUNT;
    updateLabels();
}

void uiJogUpdate(const FluidNCStatus &st)
{
    if (!posLabel) return;
    if (!st.havePos)
    {
        lv_label_set_text(posLabel, "--");
        return;
    }
    float v = axisIndex == 0 ? st.wposX : (axisIndex == 1 ? st.wposY : st.wposZ);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f", v);
    lv_label_set_text(posLabel, buf);
}
