#include "ui_home.h"
#include "../net/fluidnc_client.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <string.h>

namespace
{
    void confirmEventCb(lv_event_t *e)
    {
        lv_obj_t *mbox = lv_event_get_current_target(e);
        const char *txt = lv_msgbox_get_active_btn_text(mbox);
        if (txt && strcmp(txt, "Home") == 0)
        {
            // $X first: FluidNC won't home while alarmed, and homing after
            // an unrelated alarm is exactly the "clear the bed, then
            // home" flow this confirm exists for.
            fluidNC.clearAlarm();
            fluidNC.home();
        }
        lv_msgbox_close(mbox);
    }

    void homeBtnCb(lv_event_t *e)
    {
        (void)e;
        static const char *btns[] = {"Home", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "Confirm Homing", "Run $H now?\nMake sure the bed is clear.", btns, false);
        lv_obj_center(mbox);
        lv_obj_add_event_cb(mbox, confirmEventCb, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

lv_obj_t *uiHomeCreate()
{
    ScreenShell shell = createScreenShell("HOME", LV_SYMBOL_HOME);

    lv_obj_t *desc = lv_label_create(shell.content);
    lv_label_set_text(desc, "Send the machine\nhome ($H)");
    lv_obj_set_style_text_align(desc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(desc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(desc, Palette::border(), 0);

    lv_obj_t *btn = lv_btn_create(shell.content);
    lv_obj_set_size(btn, 100, 44);
    lv_obj_set_style_bg_color(btn, Palette::accent(), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, homeBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "$H");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, Palette::accentFg(), 0); // accent() is a light pastel now -- needs a dark fg
    lv_obj_center(lbl);

    return shell.screen;
}
