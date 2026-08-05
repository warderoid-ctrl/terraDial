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

    lv_obj_t *upSeg = nullptr;
    lv_obj_t *downSeg = nullptr;
    lv_obj_t *upLbl = nullptr;
    lv_obj_t *downLbl = nullptr;

    void restyleSegments()
    {
        lv_obj_set_style_bg_color(upSeg, penIsUp ? Palette::accent() : Palette::bgTerminal(), 0);
        lv_obj_set_style_text_color(upLbl, penIsUp ? Palette::accentFg() : Palette::textMuted(), 0);
        lv_obj_set_style_bg_color(downSeg, !penIsUp ? Palette::accent() : Palette::bgTerminal(), 0);
        lv_obj_set_style_text_color(downLbl, !penIsUp ? Palette::accentFg() : Palette::textMuted(), 0);
    }

    void setPenUp(bool up)
    {
        if (up == penIsUp) return;
        penIsUp = up;
        // Flat relative Z jog (distance/feed editable on the Settings
        // screen). Positive when the new state is "up" (away from the bed).
        const AppSettings &cfg = Config::get();
        float deltaMm = penIsUp ? cfg.penJogMm : -cfg.penJogMm;
        fluidNC.jog('Z', deltaMm, cfg.penJogFeed);
        restyleSegments();
    }

    void upSegCb(lv_event_t *e) { (void)e; setPenUp(true); }
    void downSegCb(lv_event_t *e) { (void)e; setPenUp(false); }
}

lv_obj_t *uiPenCreate()
{
    ScreenShell shell = createScreenShell("PEN", LV_SYMBOL_EDIT);

    // Segmented control (not a toggle switch): both states are always
    // visible with the active one accent-filled, so the current state
    // reads at a glance rather than needing to infer it from a single
    // button's label.
    lv_obj_t *seg = lv_obj_create(shell.content);
    lv_obj_set_size(seg, 170, 40);
    lv_obj_set_style_bg_color(seg, Palette::bgPanel(), 0);
    lv_obj_set_style_radius(seg, 20, 0);
    lv_obj_set_style_border_width(seg, 0, 0);
    lv_obj_set_style_pad_all(seg, 3, 0);
    lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    upSeg = lv_obj_create(seg);
    lv_obj_set_size(upSeg, lv_pct(48), lv_pct(100));
    lv_obj_set_style_radius(upSeg, 17, 0);
    lv_obj_set_style_border_width(upSeg, 0, 0);
    lv_obj_clear_flag(upSeg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(upSeg, upSegCb, LV_EVENT_CLICKED, NULL);
    upLbl = lv_label_create(upSeg);
    lv_label_set_text(upLbl, "Pen up");
    lv_obj_set_style_text_font(upLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(upLbl);

    downSeg = lv_obj_create(seg);
    lv_obj_set_size(downSeg, lv_pct(48), lv_pct(100));
    lv_obj_set_style_radius(downSeg, 17, 0);
    lv_obj_set_style_border_width(downSeg, 0, 0);
    lv_obj_clear_flag(downSeg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(downSeg, downSegCb, LV_EVENT_CLICKED, NULL);
    downLbl = lv_label_create(downSeg);
    lv_label_set_text(downLbl, "Pen down");
    lv_obj_set_style_text_font(downLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(downLbl);

    restyleSegments();
    return shell.screen;
}

void uiPenToggle() { setPenUp(!penIsUp); }
bool uiPenIsDown() { return !penIsUp; }
