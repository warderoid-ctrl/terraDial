#include "ui_widgets.h"
#include "palette.h"

lv_obj_t *uiMakeRow(lv_obj_t *parent, const char *labelText, lv_obj_t **outLabel)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    // A row is a passive container -- it must never scroll or draw a
    // scrollbar. Left scrollable, any child a few px wider than the row
    // (e.g. a line of chips) tips it into horizontal overflow, and LVGL
    // then draws a translucent grey scrollbar straight across that child.
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    if (labelText)
    {
        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, labelText);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, Palette::textMuted(), 0);
        if (outLabel) *outLabel = lbl;
    }

    return row;
}

lv_obj_t *uiMakePanel(lv_obj_t *parent, const char *title)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 240, 240);
    lv_obj_center(panel);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0); // the screen behind already carries the background
    lv_obj_set_style_radius(panel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_hor(panel, 30, 0);
    lv_obj_set_style_pad_top(panel, 46, 0);
    lv_obj_set_style_pad_bottom(panel, 56, 0); // clears the back button
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // No scrollbar: a straight bar down the edge of a CIRCULAR panel can't
    // hug anything, it just cuts across the face and reads as a rendering
    // fault. Scrolling still works by knob and by drag.
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    if (title)
    {
        lv_obj_t *titleLbl = lv_label_create(panel);
        lv_label_set_text(titleLbl, title);
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(titleLbl, Palette::accentSecondary(), 0);
    }
    return panel;
}

lv_obj_t *uiMakeSlider(lv_obj_t *parent, int32_t min, int32_t max, int32_t value)
{
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, lv_pct(100));
    lv_obj_set_height(slider, 10); // chunkier than stock -- easier to grab on a small round panel
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);

    // Unfilled track
    lv_obj_set_style_bg_color(slider, Palette::bgPanel(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, 5, LV_PART_MAIN);
    // Filled portion
    lv_obj_set_style_bg_color(slider, Palette::accent(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, 5, LV_PART_INDICATOR);
    // Handle -- white on the red reads clearly and matches accentFg usage
    lv_obj_set_style_bg_color(slider, Palette::accentFg(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    // Extends the touch area past the 10px track without drawing bigger.
    lv_obj_set_ext_click_area(slider, 8);
    return slider;
}

lv_obj_t *uiMakeSwitch(lv_obj_t *parent, bool checked)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_size(sw, 44, 24);
    lv_obj_set_style_bg_color(sw, Palette::bgPanel(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, Palette::accent(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, Palette::accentFg(), LV_PART_KNOB);
    lv_obj_set_ext_click_area(sw, 8);
    if (checked) lv_obj_add_state(sw, LV_STATE_CHECKED);
    return sw;
}

lv_obj_t *uiMakeButton(lv_obj_t *parent, const char *text, lv_obj_t **outLabel)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, 34);
    lv_obj_set_style_radius(btn, 17, 0);
    lv_obj_set_style_bg_color(btn, Palette::accent(), 0);
    lv_obj_set_style_bg_color(btn, Palette::accentHover(), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, Palette::accentFg(), 0);
    lv_obj_center(lbl);
    if (outLabel) *outLabel = lbl;
    return btn;
}

UiRingIconSize uiRingIconSize(float nearness, UiRingIconSize current)
{
    if (nearness > (current >= UiRingIconLarge ? 0.80f : 0.90f)) return UiRingIconLarge;
    if (nearness > (current >= UiRingIconMedium ? 0.42f : 0.52f)) return UiRingIconMedium;
    return UiRingIconSmall;
}

const lv_font_t *uiRingIconFont(UiRingIconSize size)
{
    switch (size)
    {
        case UiRingIconLarge:  return &lv_font_montserrat_32;
        case UiRingIconMedium: return &lv_font_montserrat_24;
        case UiRingIconSmall:
        default:               return &lv_font_montserrat_14;
    }
}
