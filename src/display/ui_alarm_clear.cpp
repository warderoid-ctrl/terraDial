#include "ui_alarm_clear.h"
#include "../net/fluidnc_client.h"
#include "palette.h"

// The mockup shows the specific alarm reason ("Hard limit triggered ·
// Z-"), but FluidNC's status-report parser (fluidnc_client.cpp) only
// extracts machine mode/position/job progress, not an alarm cause -- that
// would need FluidNC's separate ALARM: message line parsed too, which
// isn't done today. Shows a generic message rather than inventing a
// reason.
namespace
{
    void clearBtnCb(lv_event_t *e) { (void)e; uiAlarmClearTrigger(); }
}

lv_obj_t *uiAlarmClearCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    lv_obj_t *iconLbl = lv_label_create(scr);
    lv_label_set_text(iconLbl, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(iconLbl, Palette::accentSecondary(), 0);
    lv_obj_align(iconLbl, LV_ALIGN_CENTER, 0, -46);

    lv_obj_t *titleLbl = lv_label_create(scr);
    lv_label_set_text(titleLbl, "Alarm active");
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(titleLbl, lv_color_white(), 0);
    lv_obj_align(titleLbl, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *bodyLbl = lv_label_create(scr);
    lv_label_set_text(bodyLbl, "Clear the bed, then clear\nthe alarm to continue.");
    lv_obj_set_style_text_align(bodyLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(bodyLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bodyLbl, Palette::textMuted(), 0);
    lv_obj_align(bodyLbl, LV_ALIGN_CENTER, 0, 24);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 150, 44);
    lv_obj_set_style_bg_color(btn, Palette::accentSecondary(), 0);
    lv_obj_set_style_radius(btn, 22, 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -34);
    lv_obj_add_event_cb(btn, clearBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnLbl = lv_label_create(btn);
    lv_label_set_text(btnLbl, "Clear alarm");
    lv_obj_set_style_text_color(btnLbl, Palette::accentFg(), 0);
    lv_obj_center(btnLbl);

    return scr;
}

void uiAlarmClearTrigger()
{
    fluidNC.clearAlarm();
}
