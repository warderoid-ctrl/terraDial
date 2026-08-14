#include "ui_brand.h"
#include "icon_logo.h"
#include "palette.h"
#include "branding.h"

namespace
{
    lv_obj_t *overlay = nullptr;
}

namespace UiBrand
{
    void show()
    {
        if (overlay) return;

        overlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(overlay, 240, 240);
        lv_obj_center(overlay);
        lv_obj_set_style_bg_color(overlay, Palette::bgApp(), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(overlay, 0, 0);
        lv_obj_set_style_radius(overlay, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
        // Deliberately NOT clickable: dismissal is handled centrally by
        // ScreenSleep so the dismissing touch is swallowed, exactly as it is
        // when waking from sleep. Letting this overlay eat the click itself
        // would mean two different paths doing the same job.
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *logo = lv_img_create(overlay);
        lv_img_set_src(logo, &iconLogo);
        // Alpha-only source: without recolor_opa it draws as nothing. This is
        // also what "inverts" the mark -- the artwork is a black stroke for
        // print, and here it's painted light on the dark face.
        lv_obj_set_style_img_recolor(logo, Palette::text(), 0);
        lv_obj_set_style_img_recolor_opa(logo, LV_OPA_COVER, 0);
        lv_obj_align(logo, LV_ALIGN_CENTER, 0, -18);

        lv_obj_t *nameLbl = lv_label_create(overlay);
        lv_label_set_text(nameLbl, Branding::productName());
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(nameLbl, Palette::text(), 0);
        lv_obj_align(nameLbl, LV_ALIGN_CENTER, 0, 62);

        lv_obj_t *siteLbl = lv_label_create(overlay);
        lv_label_set_text(siteLbl, Branding::siteLabel());
        lv_obj_set_style_text_font(siteLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(siteLbl, Palette::accent(), 0);
        lv_obj_align(siteLbl, LV_ALIGN_CENTER, 0, 84);
    }

    void hide()
    {
        if (!overlay) return;
        lv_obj_del(overlay);
        overlay = nullptr;
    }

    bool isShown() { return overlay != nullptr; }
}
