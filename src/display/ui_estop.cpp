#include "ui_estop.h"
#include "../net/fluidnc_client.h"
#include "palette.h"
#include "ui_screen_shell.h"

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

    // Button sized to leave the back button clear at the bottom of the
    // circle. The "feed hold + soft reset" caption lives INSIDE the button
    // (as in the mockup): as a sibling anchored to the screen bottom it
    // landed on top of the red circle, since a 172px circle centered on a
    // 240px screen already reaches down to y=206.
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 156, 156);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, Palette::alert(), 0);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -14);
    lv_obj_add_event_cb(btn, estopBtnCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *iconLbl = lv_label_create(btn);
    lv_label_set_text(iconLbl, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(iconLbl, lv_color_white(), 0);
    lv_obj_align(iconLbl, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *nameLbl = lv_label_create(btn);
    lv_label_set_text(nameLbl, "E-STOP");
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(nameLbl, lv_color_white(), 0);
    lv_obj_align(nameLbl, LV_ALIGN_CENTER, 0, -2);

    lv_obj_t *subLbl = lv_label_create(btn);
    lv_label_set_text(subLbl, "Feed hold\n+ soft reset");
    lv_obj_set_style_text_align(subLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subLbl, lv_color_white(), 0);
    lv_obj_set_style_text_opa(subLbl, LV_OPA_80, 0);
    lv_obj_align(subLbl, LV_ALIGN_CENTER, 0, 32);

    addBackButton(scr);

    return scr;
}

void uiEstopTrigger()
{
    fluidNC.feedHold();
    fluidNC.softReset();
}
