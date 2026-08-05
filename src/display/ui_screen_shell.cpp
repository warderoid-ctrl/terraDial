#include "ui_screen_shell.h"
#include "palette.h"
#include "ui_nav.h"

namespace
{
    void backBtnCb(lv_event_t *e) { (void)e; UiNav::goHome(); }
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

ScreenShell createScreenShell(const char *title, const char *icon)
{
    ScreenShell shell;

    shell.screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(shell.screen, Palette::bgApp(), 0);

    shell.ring = lv_arc_create(shell.screen);
    lv_obj_set_size(shell.ring, 224, 224);
    lv_obj_center(shell.ring);
    lv_arc_set_bg_angles(shell.ring, 0, 360);
    lv_arc_set_value(shell.ring, 100);
    lv_obj_set_style_arc_color(shell.ring, Palette::border(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(shell.ring, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(shell.ring, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(shell.ring, Palette::bgPanel(), LV_PART_MAIN);
    lv_obj_remove_style(shell.ring, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(shell.ring, LV_OBJ_FLAG_CLICKABLE);

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

    addBackButton(shell.screen);

    return shell;
}
