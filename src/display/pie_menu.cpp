#include "pie_menu.h"
#include "palette.h"
#include <math.h>

// Icons are LV_SYMBOL_* built-ins for now, standing in for the real Lucide
// glyphs (Crosshair/FileText/PenLine/Palette/House/CircleSlash2) terraForge
// uses -- matching those exactly means converting SVG glyphs into an LVGL
// font/image asset, which is a follow-up pass once this wedge mechanic
// itself is confirmed to feel right on the hardware.
//
// This widget is written specifically for the 240x240 round CrowPanel
// (not a generic reusable control), so the dial center is just hardcoded
// to the screen center rather than computed from parent layout.
namespace
{
    const lv_coord_t DIAL_CENTER_X = 120;
    const lv_coord_t DIAL_CENTER_Y = 120;

    // "Clockwise degrees from top" (how the mockup/spec thinks about wedge
    // position) -> LVGL's mask angle convention (0 deg on the right,
    // increasing clockwise).
    lv_coord_t fromTopToLvglAngle(float degFromTop)
    {
        float a = degFromTop - 90.0f;
        while (a < 0) a += 360.0f;
        while (a >= 360.0f) a -= 360.0f;
        return (lv_coord_t)a;
    }
}

void PieMenu::create(lv_obj_t *parent, const PieMenuItem *items, int count,
                      lv_coord_t outerRadius, lv_coord_t innerRadius)
{
    itemCount_ = count > MAX_ITEMS ? MAX_ITEMS : count;
    outerRadius_ = outerRadius;
    innerRadius_ = innerRadius;
    centerX_ = DIAL_CENTER_X;
    centerY_ = DIAL_CENTER_Y;

    const float wedgeWidthDeg = 360.0f / itemCount_;
    const float gapDeg = 3.0f; // visual gap only -- hit-testing uses the full slot

    for (int i = 0; i < itemCount_; i++)
    {
        Wedge &w = wedges_[i];
        wedgeCtx_[i].menu = this;
        wedgeCtx_[i].index = i;
        labels_[i] = items[i].label;

        float centerDeg = i * wedgeWidthDeg; // wedge 0 centered at top
        w.startAngle = fromTopToLvglAngle(centerDeg - wedgeWidthDeg / 2.0f + gapDeg / 2.0f);
        w.endAngle = fromTopToLvglAngle(centerDeg + wedgeWidthDeg / 2.0f - gapDeg / 2.0f);

        w.bg = lv_obj_create(parent);
        lv_obj_set_size(w.bg, outerRadius_ * 2, outerRadius_ * 2);
        lv_obj_center(w.bg);
        lv_obj_set_style_radius(w.bg, LV_RADIUS_CIRCLE, 0); // caps the outer edge to a circle
        lv_obj_set_style_border_width(w.bg, 0, 0);
        lv_obj_clear_flag(w.bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(w.bg, LV_OBJ_FLAG_CLICKABLE); // hit-testing is via the overlay, not this
        lv_obj_add_event_cb(w.bg, wedgeDrawBeginCb, LV_EVENT_DRAW_MAIN_BEGIN, &wedgeCtx_[i]);
        lv_obj_add_event_cb(w.bg, wedgeDrawEndCb, LV_EVENT_DRAW_MAIN_END, &wedgeCtx_[i]);

        float midRad = (centerDeg) * (float)M_PI / 180.0f;
        float labelR = (outerRadius_ + innerRadius_) / 2.0f;
        lv_coord_t lx = centerX_ + (lv_coord_t)(labelR * sinf(midRad));
        lv_coord_t ly = centerY_ - (lv_coord_t)(labelR * cosf(midRad));

        w.iconLbl = lv_label_create(parent);
        lv_label_set_text(w.iconLbl, items[i].icon);
        lv_obj_set_style_text_font(w.iconLbl, &lv_font_montserrat_18, 0);
        lv_obj_align(w.iconLbl, LV_ALIGN_TOP_LEFT, lx - 8, ly - 16);
        lv_obj_clear_flag(w.iconLbl, LV_OBJ_FLAG_CLICKABLE);

        w.nameLbl = lv_label_create(parent);
        lv_label_set_text(w.nameLbl, items[i].label);
        lv_obj_set_style_text_font(w.nameLbl, &lv_font_montserrat_12, 0);
        lv_obj_align(w.nameLbl, LV_ALIGN_TOP_LEFT, lx - 20, ly + 6);
        lv_obj_clear_flag(w.nameLbl, LV_OBJ_FLAG_CLICKABLE);
    }

    hub_ = lv_obj_create(parent);
    lv_obj_set_size(hub_, innerRadius_ * 2, innerRadius_ * 2);
    lv_obj_center(hub_);
    lv_obj_set_style_radius(hub_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub_, Palette::bgTerminal(), 0);
    lv_obj_set_style_bg_opa(hub_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hub_, 2, 0);
    lv_obj_set_style_border_color(hub_, Palette::border(), 0);
    lv_obj_set_style_border_opa(hub_, LV_OPA_50, 0);
    lv_obj_clear_flag(hub_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hub_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hub_, overlayEventCb, LV_EVENT_CLICKED, this);

    hubTitleLbl_ = lv_label_create(hub_);
    lv_obj_set_style_text_font(hubTitleLbl_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hubTitleLbl_, Palette::textMuted(), 0);
    lv_obj_align(hubTitleLbl_, LV_ALIGN_TOP_MID, 0, 14);

    hubMainLbl_ = lv_label_create(hub_);
    lv_obj_set_style_text_font(hubMainLbl_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(hubMainLbl_, lv_color_white(), 0);
    lv_obj_align(hubMainLbl_, LV_ALIGN_CENTER, 0, -4);

    hubHintLbl_ = lv_label_create(hub_);
    lv_obj_set_style_text_font(hubHintLbl_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hubHintLbl_, Palette::accent(), 0);
    lv_obj_align(hubHintLbl_, LV_ALIGN_BOTTOM_MID, 0, -12);

    // Invisible full-dial overlay: the actual tap target. Sits above the
    // wedge backgrounds (created after them) but below the hub, which
    // handles its own clicks separately as "go back".
    overlay_ = lv_obj_create(parent);
    lv_obj_set_size(overlay_, outerRadius_ * 2, outerRadius_ * 2);
    lv_obj_center(overlay_);
    lv_obj_set_style_radius(overlay_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay_, 0, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(overlay_, overlayEventCb, LV_EVENT_CLICKED, this);

    // The hub sits visually inside the overlay's bounding circle but is a
    // separate, later-created (so higher z-order) clickable object with its
    // own handler -- taps within its radius never reach the overlay.

    restyleSelection();
}

void PieMenu::restyleWedge(int index, bool selected)
{
    Wedge &w = wedges_[index];
    lv_obj_set_style_bg_color(w.bg, selected ? Palette::selectedFill() : Palette::bgPanel(), 0);
    lv_obj_set_style_bg_opa(w.bg, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(w.nameLbl, selected ? lv_color_white() : Palette::border(), 0);
    lv_obj_set_style_text_color(w.iconLbl, selected ? Palette::accent() : Palette::border(), 0);
    lv_obj_set_style_border_width(w.bg, selected ? 2 : 0, 0);
    lv_obj_set_style_border_color(w.bg, Palette::accent(), 0);
    lv_obj_set_style_border_opa(w.bg, LV_OPA_COVER, 0);
}

void PieMenu::restyleSelection()
{
    // Each wedge does real per-pixel angle+radius mask compositing in
    // software (no hardware acceleration) -- lv_obj_set_style_* invalidates
    // an object regardless of whether the value actually changed, so
    // touching all N wedges on every single detent was forcing N expensive
    // masked redraws when only 2 ever actually change appearance. That was
    // the direct cause of laggy/inconsistent-feeling knob response
    // (confirmed on hardware). Only restyle the wedges whose selected state
    // is actually different from last time.
    if (styledSelectedIndex_ != selectedIndex_)
    {
        if (styledSelectedIndex_ >= 0) restyleWedge(styledSelectedIndex_, false);
        restyleWedge(selectedIndex_, true);
        styledSelectedIndex_ = selectedIndex_;
    }

    lv_label_set_text(hubMainLbl_, labels_[selectedIndex_]);
    lv_label_set_text(hubHintLbl_, "press to open");
}

void PieMenu::selectNext()
{
    selectedIndex_ = (selectedIndex_ + 1) % itemCount_;
    restyleSelection();
}

void PieMenu::selectPrev()
{
    selectedIndex_ = (selectedIndex_ - 1 + itemCount_) % itemCount_;
    restyleSelection();
}

void PieMenu::setHubTitle(const char *title, lv_color_t color)
{
    lv_label_set_text(hubTitleLbl_, title);
    lv_obj_set_style_text_color(hubTitleLbl_, color, 0);
}

void PieMenu::setHubHint(const char *hint)
{
    lv_label_set_text(hubHintLbl_, hint);
}

void PieMenu::handleTap(lv_coord_t x, lv_coord_t y)
{
    float dx = x - centerX_;
    float dy = y - centerY_;
    float radius = sqrtf(dx * dx + dy * dy);
    if (radius > outerRadius_) return; // outside the dial entirely

    float angleFromRight = atan2f(dy, dx) * 180.0f / (float)M_PI; // LVGL convention already
    if (angleFromRight < 0) angleFromRight += 360.0f;
    float angleFromTop = angleFromRight + 90.0f;
    if (angleFromTop >= 360.0f) angleFromTop -= 360.0f;

    float wedgeWidthDeg = 360.0f / itemCount_;
    int idx = (int)((angleFromTop + wedgeWidthDeg / 2.0f) / wedgeWidthDeg) % itemCount_;

    if (idx == selectedIndex_)
    {
        if (onOpen_) onOpen_(selectedIndex_);
    }
    else
    {
        selectedIndex_ = idx;
        restyleSelection();
    }
}

void PieMenu::overlayEventCb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    PieMenu *self = (PieMenu *)lv_event_get_user_data(e);

    if (target == self->hub_)
    {
        if (self->onBack_) self->onBack_();
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    self->handleTap(p.x, p.y);
}

void PieMenu::wedgeDrawBeginCb(lv_event_t *e)
{
    PieMenu::WedgeCbCtx *ctx = (PieMenu::WedgeCbCtx *)lv_event_get_user_data(e);
    PieMenu::Wedge &w = ctx->menu->wedges_[ctx->index];

    lv_draw_mask_angle_init(&w.angleMaskParam, ctx->menu->centerX_, ctx->menu->centerY_, w.startAngle, w.endAngle);
    w.angleMaskId = lv_draw_mask_add(&w.angleMaskParam, NULL);

    lv_coord_t ir = ctx->menu->innerRadius_;
    lv_area_t holeRect;
    holeRect.x1 = ctx->menu->centerX_ - ir;
    holeRect.y1 = ctx->menu->centerY_ - ir;
    holeRect.x2 = ctx->menu->centerX_ + ir;
    holeRect.y2 = ctx->menu->centerY_ + ir;
    lv_draw_mask_radius_init(&w.holeMaskParam, &holeRect, ir, true); // true: keep pixels OUTSIDE (punch the hub hole)
    w.holeMaskId = lv_draw_mask_add(&w.holeMaskParam, NULL);
}

void PieMenu::wedgeDrawEndCb(lv_event_t *e)
{
    PieMenu::WedgeCbCtx *ctx = (PieMenu::WedgeCbCtx *)lv_event_get_user_data(e);
    PieMenu::Wedge &w = ctx->menu->wedges_[ctx->index];

    if (w.angleMaskId >= 0) { lv_draw_mask_remove_id(w.angleMaskId); w.angleMaskId = -1; }
    if (w.holeMaskId >= 0) { lv_draw_mask_remove_id(w.holeMaskId); w.holeMaskId = -1; }
}
