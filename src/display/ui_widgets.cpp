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
