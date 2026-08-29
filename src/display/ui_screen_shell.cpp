#include "ui_screen_shell.h"
#include "palette.h"
#include "ui_nav.h"

namespace
{
    void backBtnCb(lv_event_t *e) { (void)e; UiNav::goHome(); }
    void estopPipCb(lv_event_t *e) { (void)e; UiNav::goEstop(); }
}

void addBackButton(lv_obj_t *screen)
{
    // y=-10 from the bottom edge (screen center to button center distance
    // ~110px) -- inside the round panel's ~120px visible radius, matching
    // the safe-inset convention the E-Stop/Alarm Clear/Job Progress
    // screens already use for their own bottom-anchored content.
    lv_obj_t *btn = lv_btn_create(screen);
    lv_obj_set_size(btn, 36, 36);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, Palette::bgSecondary(), 0);
    lv_obj_set_ext_click_area(btn, 10); // easier to hit near the curved bezel
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_event_cb(btn, backBtnCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lbl, Palette::textMuted(), 0);
    lv_obj_center(lbl);
}

void addEstopButton(lv_obj_t *screen)
{
    // 26px and offset -38/-14, which is smaller and higher than it looks
    // like it should be. The panel is round: at the back button's own
    // baseline the usable half-width is about 43px, so a full-size chip
    // beside it would have its lower outside corner off the glass. Raising
    // it 6px and shrinking it 10 puts the whole pip inside the circle with
    // room to spare, and ext_click_area gives back the touch target that
    // costs.
    lv_obj_t *btn = lv_btn_create(screen);
    lv_obj_set_size(btn, 26, 26);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn, Palette::alert(), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_ext_click_area(btn, 8);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, -38, -14);
    lv_obj_add_event_cb(btn, estopPipCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_STOP);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, Palette::accentFg(), 0);
    lv_obj_center(lbl);
}

ScreenShell createScreenShell(const char *title, const char *icon)
{
    ScreenShell shell;

    shell.screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(shell.screen, Palette::bgApp(), 0);

    // No decorative border arc: it drew a pale ring (Palette::border()) just
    // inside the bezel, which read as a hard outline against the round glass
    // rather than as framing. The screens that once used it as an "active"
    // cue no longer exist.

    // Icon + title header, same position on every screen. The icon matches
    // this item's dial wedge, so opening a wedge visually "carries through"
    // to the screen it opens.
    lv_obj_t *header = lv_obj_create(shell.screen);
    lv_obj_set_size(header, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_pad_column(header, 6, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t *iconLbl = lv_label_create(header);
    lv_label_set_text(iconLbl, icon);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(iconLbl, Palette::textMuted(), 0);

    lv_obj_t *titleLbl = lv_label_create(header);
    lv_label_set_text(titleLbl, title);
    lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(titleLbl, Palette::textMuted(), 0);

    shell.content = lv_obj_create(shell.screen);
    lv_obj_set_size(shell.content, 186, 186);
    lv_obj_center(shell.content);
    lv_obj_set_style_bg_opa(shell.content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(shell.content, 0, 0);
    lv_obj_set_style_radius(shell.content, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(shell.content, true, 0);
    lv_obj_set_style_pad_hor(shell.content, 22, 0);
    lv_obj_set_style_pad_ver(shell.content, 18, 0);
    lv_obj_set_style_pad_row(shell.content, 8, 0);
    lv_obj_set_flex_flow(shell.content, LV_FLEX_FLOW_COLUMN);
    // Center children both along the main axis (as a group, vertically) and
    // the cross axis (each child horizontally) -- a flex container ignores
    // any per-child lv_obj_align/align-style call, so without this every
    // screen's content was packed into the top-left corner rather than
    // centered (confirmed as a real bug from the shell refactor, not just
    // a taste issue).
    lv_obj_set_flex_align(shell.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(shell.content, LV_DIR_VER);
    // See ui_settings.cpp: a straight scrollbar across a round panel reads
    // as a rendering fault, and scrolling works fine without it.
    lv_obj_set_scrollbar_mode(shell.content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(shell.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(shell.screen, LV_SCROLLBAR_MODE_OFF);

    addBackButton(shell.screen);

    return shell;
}
