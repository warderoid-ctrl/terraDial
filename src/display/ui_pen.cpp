#include "ui_pen.h"
#include "../net/fluidnc_client.h"
#include "../config/settings.h"
#include "palette.h"
#include "ui_screen_shell.h"

namespace
{
    // FluidNC doesn't report the servo's actual angle in its status
    // stream, so this is purely an optimistic UI-tracked assumption --
    // it can drift from the real physical state if the machine loses
    // power or is jogged by another client. Default to "up" as the
    // safer assumption at boot (a pen that's actually down when we think
    // it's up just means the next drag looks like a no-op; the reverse
    // could scratch the bed on the first jog).
    bool penIsUp = true;

    lv_obj_t *stateLabel = nullptr;
    lv_obj_t *toggleBtn = nullptr;
    lv_obj_t *toggleLbl = nullptr;

    void refresh()
    {
        lv_label_set_text(stateLabel, penIsUp ? "PEN UP" : "PEN DOWN");
        lv_obj_set_style_text_color(stateLabel, penIsUp ? lv_color_white() : Palette::accent(), 0);
        lv_label_set_text(toggleLbl, penIsUp ? "Lower" : "Raise");
    }

    void toggleBtnCb(lv_event_t *e)
    {
        (void)e;
        penIsUp = !penIsUp;
        // Flat relative Z jog (distance/feed editable on the Settings
        // screen). Positive when the new state is "up" (away from the bed).
        const AppSettings &cfg = Config::get();
        float deltaMm = penIsUp ? cfg.penJogMm : -cfg.penJogMm;
        fluidNC.jog('Z', deltaMm, cfg.penJogFeed);
        refresh();
    }
}

lv_obj_t *uiPenCreate()
{
    ScreenShell shell = createScreenShell("PEN", LV_SYMBOL_EDIT);

    stateLabel = lv_label_create(shell.content);
    lv_obj_set_style_text_font(stateLabel, &lv_font_montserrat_24, 0);

    toggleBtn = lv_btn_create(shell.content);
    lv_obj_set_size(toggleBtn, 100, 44);
    lv_obj_set_style_bg_color(toggleBtn, Palette::accent(), 0);
    lv_obj_set_style_radius(toggleBtn, 10, 0);
    lv_obj_add_event_cb(toggleBtn, toggleBtnCb, LV_EVENT_CLICKED, NULL);
    toggleLbl = lv_label_create(toggleBtn);
    lv_obj_center(toggleLbl);

    refresh();
    return shell.screen;
}
