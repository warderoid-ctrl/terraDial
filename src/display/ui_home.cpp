#include "ui_home.h"
#include "../net/fluidnc_client.h"
#include "palette.h"
#include "ui_nav.h"
#include "ui_screen_shell.h"

// Single-screen safety gate, per the mockup (07 - "Home (clear prompt)"):
// icon + "Clear the bed first" + body copy + a "Confirm & home" pill.
//
// This used to be a $H button that opened a separate lv_msgbox confirm, so
// homing took two taps across two different UI idioms. The screen already
// exists solely to be a confirmation, so the extra popup added a step
// without adding safety. Deliberately built to the same shape as
// ui_alarm_clear.cpp (icon -> title -> body -> single action pill) so the
// two confirm screens read and behave identically.
namespace
{
    void confirmBtnCb(lv_event_t *e) { (void)e; uiHomeTrigger(); }
}

lv_obj_t *uiHomeCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    // Vertical rhythm here is deliberately tight and shared with
    // ui_alarm_clear.cpp: icon / title / body / pill / back button all have
    // to fit inside a 240px circle without touching. Stacking four items
    // plus the back button leaves only a few px of slack, so these offsets
    // are load-bearing -- changing a font size or adding a body line will
    // collide with the pill (which is exactly what happened before).
    lv_obj_t *iconLbl = lv_label_create(scr);
    lv_label_set_text(iconLbl, LV_SYMBOL_HOME); // matches this item's dial icon
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(iconLbl, Palette::accent(), 0);
    lv_obj_align(iconLbl, LV_ALIGN_CENTER, 0, -58);

    lv_obj_t *titleLbl = lv_label_create(scr);
    lv_label_set_text(titleLbl, "Clear the bed first");
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(titleLbl, lv_color_white(), 0);
    lv_obj_align(titleLbl, LV_ALIGN_CENTER, 0, -28);

    lv_obj_t *bodyLbl = lv_label_create(scr);
    lv_label_set_text(bodyLbl, "Lift the pen and check the\ncarriage can move freely.");
    lv_obj_set_style_text_align(bodyLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(bodyLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bodyLbl, Palette::textMuted(), 0);
    lv_obj_align(bodyLbl, LV_ALIGN_CENTER, 0, -2);

    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 160, 38);
    lv_obj_set_style_bg_color(btn, Palette::accent(), 0); // primary action on this screen
    lv_obj_set_style_radius(btn, 19, 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -64); // below the body copy, above the back button
    lv_obj_add_event_cb(btn, confirmBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btnLbl = lv_label_create(btn);
    lv_label_set_text(btnLbl, "Confirm & home");
    lv_obj_set_style_text_font(btnLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(btnLbl, Palette::accentFg(), 0);
    lv_obj_center(btnLbl);

    addBackButton(scr);

    return scr;
}

void uiHomeTrigger()
{
    // FluidNC won't home while alarmed, and homing after an unrelated
    // alarm is exactly the "clear the bed, then home" flow this screen
    // exists for -- but the unlock belongs to the client, which knows
    // whether there is actually an alarm to clear and paces the $X and $H
    // apart. Sending $X unconditionally from here alarmed the machine when
    // homing twice in a row (see FluidNCClient::home()).
    fluidNC.home();
    // Back to the dial, whose hub shows the live machine state -- otherwise
    // confirming leaves you on an unchanged screen with no sign anything
    // happened.
    UiNav::goHome();
}
