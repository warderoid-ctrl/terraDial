#include "ui_job_progress.h"
#include "ui_pen.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <stdio.h>

// No fabricated time-remaining: FluidNC's SD:<pct>,<filename> status field
// (see fluidnc_client.cpp) only gives a percentage, not an ETA, unlike the
// mockup's "~12 min left" -- that number has no real source, so it's left
// out rather than invented.
namespace
{
    lv_obj_t *ring = nullptr;
    lv_obj_t *filenameLbl = nullptr;
    lv_obj_t *percentLbl = nullptr;
    lv_obj_t *penPillLbl = nullptr;
    lv_obj_t *pauseLbl = nullptr;

    void pauseBtnCb(lv_event_t *e) { (void)e; uiJobProgressTogglePause(); }

    void stopBtnCb(lv_event_t *e)
    {
        (void)e;
        // FluidNC has no separate "abort job cleanly" primitive -- same
        // feed-hold + soft-reset combo as E-Stop (ui_estop.cpp).
        fluidNC.feedHold();
        fluidNC.softReset();
    }
}

lv_obj_t *uiJobProgressCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    ring = lv_arc_create(scr);
    lv_obj_set_size(ring, 224, 224);
    lv_obj_center(ring);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_rotation(ring, 270); // start at 12 o'clock
    lv_arc_set_range(ring, 0, 100);
    lv_arc_set_value(ring, 0);
    lv_obj_set_style_arc_color(ring, Palette::accent(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ring, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ring, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, Palette::bgPanel(), LV_PART_MAIN);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    filenameLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(filenameLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(filenameLbl, Palette::textMuted(), 0);
    lv_obj_set_width(filenameLbl, 160);
    lv_label_set_long_mode(filenameLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(filenameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(filenameLbl, LV_ALIGN_CENTER, 0, -56);

    percentLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(percentLbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(percentLbl, lv_color_white(), 0);
    lv_obj_align(percentLbl, LV_ALIGN_CENTER, 0, -14);

    lv_obj_t *pauseBtn = lv_btn_create(scr);
    lv_obj_set_size(pauseBtn, 58, 58);
    lv_obj_set_style_radius(pauseBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pauseBtn, Palette::bgSecondary(), 0);
    lv_obj_align(pauseBtn, LV_ALIGN_CENTER, -34, 44);
    lv_obj_add_event_cb(pauseBtn, pauseBtnCb, LV_EVENT_CLICKED, NULL);
    pauseLbl = lv_label_create(pauseBtn);
    lv_label_set_text(pauseLbl, LV_SYMBOL_PAUSE);
    lv_obj_center(pauseLbl);

    lv_obj_t *stopBtn = lv_btn_create(scr);
    lv_obj_set_size(stopBtn, 58, 58);
    lv_obj_set_style_radius(stopBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(stopBtn, Palette::alert(), 0); // same feedHold+softReset as E-Stop, so same colour
    lv_obj_align(stopBtn, LV_ALIGN_CENTER, 34, 44);
    lv_obj_add_event_cb(stopBtn, stopBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *stopLbl = lv_label_create(stopBtn);
    lv_label_set_text(stopLbl, LV_SYMBOL_STOP);
    lv_obj_set_style_text_color(stopLbl, Palette::accentFg(), 0);
    lv_obj_center(stopLbl);

    penPillLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(penPillLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(penPillLbl, Palette::accentSecondary(), 0);
    lv_obj_align(penPillLbl, LV_ALIGN_BOTTOM_MID, 0, -34); // was -14 -- leaves room for the back button below it

    addBackButton(scr);

    return scr;
}

void uiJobProgressUpdate(const FluidNCStatus &st)
{
    if (!ring) return;

    int pct = st.jobPercent >= 0 ? (int)st.jobPercent : 0;
    lv_arc_set_value(ring, pct);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(percentLbl, buf);

    lv_label_set_text(filenameLbl, st.jobFilename[0] ? st.jobFilename : "--");
    lv_label_set_text(penPillLbl, uiPenIsDown() ? "pen down" : "pen up");
    lv_label_set_text(pauseLbl, st.mode == MachineMode::Hold ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
}

void uiJobProgressTogglePause()
{
    if (fluidNC.status().mode == MachineMode::Hold) fluidNC.resume();
    else fluidNC.feedHold();
}
