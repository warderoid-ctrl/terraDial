#include "ui_jog.h"
#include "jog_config.h"
#include "palette.h"
#include <stdio.h>

// X/Y/Z + step chips are tap-to-select directly (replacing the old knob-
// click-cycles-step behavior); the ± buttons jog one step via touch.
// Per the mockup, knob click now cycles axis instead, and knob rotation
// still jogs continuously. Z axis omits the 10mm step chip.
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
    lv_obj_t *axisSubLabel = nullptr;
    lv_obj_t *captionLbl = nullptr;

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
            lv_obj_set_style_bg_color(stepChips[i], sel ? Palette::accent() : Palette::bgTerminal(), 0);
            lv_obj_set_style_text_color(stepChipLbls[i], sel ? Palette::accentFg() : Palette::textMuted(), 0);
        }

        char subBuf[16];
        snprintf(subBuf, sizeof(subBuf), "%s position", AXIS_NAMES[axisIndex]);
        lv_label_set_text(axisSubLabel, subBuf);

        lv_label_set_text(captionLbl, axisIndex == 2 ? "Z axis: 0.1 / 1mm steps only" : "");
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

    void jogOneStep(int direction)
    {
        float distanceMm = JOG_STEPS[stepIndex].mm * direction;
        fluidNC.jog(AXIS_NAMES[axisIndex][0], distanceMm, JOG_STEPS[stepIndex].feedMmMin);
    }

    void axisChipCb(lv_event_t *e) { setAxis((int)(intptr_t)lv_event_get_user_data(e)); }
    void stepChipCb(lv_event_t *e) { setStep((int)(intptr_t)lv_event_get_user_data(e)); }
    void minusBtnCb(lv_event_t *e) { (void)e; jogOneStep(-1); }
    void plusBtnCb(lv_event_t *e) { (void)e; jogOneStep(1); }
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
    lv_obj_set_style_text_color(posLabel, lv_color_white(), 0);
    lv_obj_align(posLabel, LV_ALIGN_CENTER, 0, -22);
    lv_label_set_text(posLabel, "--");

    axisSubLabel = lv_label_create(scr);
    lv_obj_set_style_text_font(axisSubLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(axisSubLabel, Palette::textMuted(), 0);
    lv_obj_align_to(axisSubLabel, posLabel, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    lv_obj_t *minusBtn = lv_btn_create(scr);
    lv_obj_set_size(minusBtn, 52, 52);
    lv_obj_set_style_radius(minusBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(minusBtn, Palette::bgTerminal(), 0);
    lv_obj_align(minusBtn, LV_ALIGN_LEFT_MID, 14, 8);
    lv_obj_add_event_cb(minusBtn, minusBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *minusLbl = lv_label_create(minusBtn);
    lv_label_set_text(minusLbl, LV_SYMBOL_MINUS);
    lv_obj_center(minusLbl);

    lv_obj_t *plusBtn = lv_btn_create(scr);
    lv_obj_set_size(plusBtn, 52, 52);
    lv_obj_set_style_radius(plusBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(plusBtn, Palette::bgTerminal(), 0);
    lv_obj_align(plusBtn, LV_ALIGN_RIGHT_MID, -14, 8);
    lv_obj_add_event_cb(plusBtn, plusBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *plusLbl = lv_label_create(plusBtn);
    lv_label_set_text(plusLbl, LV_SYMBOL_PLUS);
    lv_obj_center(plusLbl);

    lv_obj_t *stepRow = lv_obj_create(scr);
    lv_obj_set_size(stepRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(stepRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stepRow, 0, 0);
    lv_obj_clear_flag(stepRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(stepRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(stepRow, 6, 0);
    lv_obj_align(stepRow, LV_ALIGN_BOTTOM_MID, 0, -48);

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

    captionLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(captionLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(captionLbl, Palette::border(), 0);
    lv_obj_align(captionLbl, LV_ALIGN_BOTTOM_MID, 0, -16);

    restyleChips();
    return scr;
}

void uiJogHandleRotate(int32_t delta)
{
    if (delta == 0) return;
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
