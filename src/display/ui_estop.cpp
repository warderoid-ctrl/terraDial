#include "ui_estop.h"
#include "../net/fluidnc_client.h"
#include "palette.h"

// Always reachable as a dial destination, one button, no confirm dialog --
// a confirm step would defeat the point of an emergency stop. The mockup
// also wants this reachable via a double-press on a separate physical
// touch button "from anywhere"; that hardware doesn't exist on terraTouch
// (only the touchscreen and the encoder's own click/double-click/long-
// press -- see pins.h/encoder.h), so this is reachable via the dial only.
namespace
{
    void estopBtnCb(lv_event_t *e) { (void)e; uiEstopTrigger(); }
}

lv_obj_t *uiEstopCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 172, 172);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, Palette::alert(), 0);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, estopBtnCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *iconLbl = lv_label_create(btn);
    lv_label_set_text(iconLbl, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(iconLbl, lv_color_white(), 0);
    lv_obj_align(iconLbl, LV_ALIGN_CENTER, 0, -14);

    lv_obj_t *nameLbl = lv_label_create(btn);
    lv_label_set_text(nameLbl, "E-STOP");
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(nameLbl, lv_color_white(), 0);
    lv_obj_align(nameLbl, LV_ALIGN_CENTER, 0, 16);

    lv_obj_t *subLbl = lv_label_create(scr);
    lv_label_set_text(subLbl, "Feed hold + soft reset");
    lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subLbl, Palette::border(), 0);
    lv_obj_align(subLbl, LV_ALIGN_BOTTOM_MID, 0, -30);

    return scr;
}

void uiEstopTrigger()
{
    fluidNC.feedHold();
    fluidNC.softReset();
}
