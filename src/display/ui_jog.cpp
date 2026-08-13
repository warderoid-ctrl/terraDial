#include "ui_jog.h"
#include "jog_config.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include "../led/panel_ring.h"
#include <stdio.h>

// X/Y/Z + step chips are tap-to-select directly (replacing the old knob-
// click-cycles-step behavior). Knob rotation jogs continuously (the ring's
// own +/- motion), so there's no separate on-screen +/- control -- knob
// click cycles axis instead. Z axis omits the 10mm step chip.
namespace
{
    const char *AXIS_NAMES[] = {"X", "Y", "Z"};
    const int AXIS_COUNT = 3;

    int stepIndex = 1; // default 1.0mm
    int axisIndex = 0; // default X

    lv_obj_t *axisChips[AXIS_COUNT] = {nullptr};
    lv_obj_t *axisChipLbls[AXIS_COUNT] = {nullptr};
    lv_obj_t *stepChips[JOG_STEP_COUNT] = {nullptr};
    lv_obj_t *stepChipLbls[JOG_STEP_COUNT] = {nullptr};
    lv_obj_t *posLabel = nullptr;
    lv_obj_t *setZeroBtn = nullptr;
    lv_obj_t *setZeroLbl = nullptr;

    void restyleChips()
    {
        for (int i = 0; i < AXIS_COUNT; i++)
        {
            bool sel = i == axisIndex;
            lv_obj_set_style_bg_opa(axisChips[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(axisChips[i], Palette::accent(), 0);
            lv_obj_set_style_text_color(axisChipLbls[i], sel ? Palette::accentFg() : Palette::border(), 0);
        }
        for (int i = 0; i < JOG_STEP_COUNT; i++)
        {
            bool isTenMmChip = (i == JOG_STEP_COUNT - 1);
            if (axisIndex == 2 && isTenMmChip)
            {
                lv_obj_add_flag(stepChips[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_clear_flag(stepChips[i], LV_OBJ_FLAG_HIDDEN);
            bool sel = i == stepIndex;
            lv_obj_set_style_bg_color(stepChips[i], sel ? Palette::accent() : Palette::bgSecondary(), 0);
            lv_obj_set_style_text_color(stepChipLbls[i], sel ? Palette::accentFg() : Palette::textMuted(), 0);
        }

        char zeroBuf[20];
        snprintf(zeroBuf, sizeof(zeroBuf), "Set %s zero", AXIS_NAMES[axisIndex]);
        lv_label_set_text(setZeroLbl, zeroBuf);
    }

    void setAxis(int idx)
    {
        if (idx == axisIndex) return;
        axisIndex = idx;
        // Z has no 10mm step -- clamp back if it was selected elsewhere.
        if (axisIndex == 2 && stepIndex >= JOG_STEP_COUNT - 1) stepIndex = JOG_STEP_COUNT - 2;
        restyleChips();
    }

    void setStep(int idx)
    {
        if (idx == stepIndex) return;
        stepIndex = idx;
        restyleChips();
    }

    void axisChipCb(lv_event_t *e) { setAxis((int)(intptr_t)lv_event_get_user_data(e)); }
    void stepChipCb(lv_event_t *e) { setStep((int)(intptr_t)lv_event_get_user_data(e)); }

    void setZeroCb(lv_event_t *e)
    {
        (void)e;
        // G10 L20 P0 <axis>0 = "the current position of this axis is now
        // zero in the active work coordinate system". P0 means the WCS
        // currently in effect, so this follows whatever G54-G59 is active
        // rather than assuming G54. Only the selected axis is touched --
        // the others keep their existing offsets.
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "G10 L20 P0 %s0", AXIS_NAMES[axisIndex]);
        fluidNC.sendGcodeLine(cmd);
    }
}

lv_obj_t *uiJogCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    lv_obj_t *axisRow = lv_obj_create(scr);
    lv_obj_set_size(axisRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(axisRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(axisRow, 0, 0);
    lv_obj_clear_flag(axisRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(axisRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(axisRow, 6, 0);
    lv_obj_align(axisRow, LV_ALIGN_TOP_MID, 0, 32);

    for (int i = 0; i < AXIS_COUNT; i++)
    {
        lv_obj_t *chip = lv_obj_create(axisRow);
        lv_obj_set_size(chip, 34, 26);
        lv_obj_set_style_radius(chip, 13, 0);
        lv_obj_set_style_border_width(chip, 0, 0);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(chip, axisChipCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *lbl = lv_label_create(chip);
        lv_label_set_text(lbl, AXIS_NAMES[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
        axisChips[i] = chip;
        axisChipLbls[i] = lbl;
    }

    posLabel = lv_label_create(scr);
    lv_obj_set_style_text_font(posLabel, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(posLabel, Palette::text(), 0);
    lv_obj_align(posLabel, LV_ALIGN_CENTER, 0, -30);
    lv_label_set_text(posLabel, "--");

    // No "X position" caption under the readout: the highlighted axis chip
    // above already says which axis this is, so it was restating the UI at
    // the cost of the vertical space Set Zero needed.

    // Zeroes whichever axis is currently selected, so each axis gets its own
    // "set zero" without needing three separate buttons competing for space.
    // Label updates with the axis (see restyleChips) so it always states
    // exactly what it will do.
    setZeroBtn = lv_btn_create(scr);
    lv_obj_set_size(setZeroBtn, 92, 24);
    lv_obj_set_style_radius(setZeroBtn, 12, 0);
    lv_obj_set_style_bg_color(setZeroBtn, Palette::bgSecondary(), 0);
    lv_obj_set_style_bg_color(setZeroBtn, Palette::accent(), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(setZeroBtn, 0, 0);
    lv_obj_set_ext_click_area(setZeroBtn, 6);
    lv_obj_align(setZeroBtn, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_event_cb(setZeroBtn, setZeroCb, LV_EVENT_CLICKED, NULL);
    setZeroLbl = lv_label_create(setZeroBtn);
    lv_obj_set_style_text_font(setZeroLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(setZeroLbl, Palette::textMuted(), 0);
    lv_obj_center(setZeroLbl);

    lv_obj_t *stepRow = lv_obj_create(scr);
    lv_obj_set_size(stepRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(stepRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stepRow, 0, 0);
    lv_obj_clear_flag(stepRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(stepRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(stepRow, 6, 0);
    lv_obj_align(stepRow, LV_ALIGN_BOTTOM_MID, 0, -52); // clears the Set Zero button above

    for (int i = 0; i < JOG_STEP_COUNT; i++)
    {
        lv_obj_t *chip = lv_obj_create(stepRow);
        lv_obj_set_size(chip, 40, 28);
        lv_obj_set_style_radius(chip, 14, 0);
        lv_obj_set_style_border_width(chip, 0, 0);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(chip, stepChipCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *lbl = lv_label_create(chip);
        char buf[8];
        if (JOG_STEPS[i].mm < 1.0f) snprintf(buf, sizeof(buf), "%.1f", JOG_STEPS[i].mm);
        else snprintf(buf, sizeof(buf), "%.0f", JOG_STEPS[i].mm);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        stepChips[i] = chip;
        stepChipLbls[i] = lbl;
    }

    // No "Z axis: 0.1 / 1mm steps only" caption: it sat under the back
    // button, and the step chips already show the truth by hiding the 10mm
    // chip on Z -- the sentence just restated what the row was doing.
    addBackButton(scr);

    restyleChips();
    return scr;
}

void uiJogHandleRotate(int32_t delta)
{
    if (delta == 0) return;
    // Make the LED ring sweep the same way the knob turned: + clockwise,
    // - counter-clockwise.
    panelRing.setChaseDirection(delta > 0 ? 1 : -1);
    // delta carries direction and the encoder's own fast-turn weighting
    // (see input/encoder.cpp) -- used directly as a step multiplier so a
    // quick flick moves further per tick than a slow turn.
    float distanceMm = JOG_STEPS[stepIndex].mm * delta;
    fluidNC.jog(AXIS_NAMES[axisIndex][0], distanceMm, JOG_STEPS[stepIndex].feedMmMin);
}

void uiJogCycleAxis()
{
    setAxis((axisIndex + 1) % AXIS_COUNT);
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
