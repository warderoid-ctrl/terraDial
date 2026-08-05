#pragma once
#include <lvgl.h>

struct PieMenuItem
{
    const char *label;
    const char *icon; // LV_SYMBOL_* placeholder -- see pie_menu.cpp header comment
};

// A tappable radial wedge selector: items arranged evenly around a ring,
// with a center "hub" showing context info. Two ways to open the
// highlighted item: tap it a second time, or press the knob (rotate to
// move the highlight, click to open) -- both drive the same selection
// state, so touch and the physical jog wheel are equivalent inputs.
//
// Visual wedges are real angle+radius masked lv_obj backgrounds (so they
// render as true pie slices, not approximated rectangles), but hit-testing
// is done separately and more simply: a single invisible overlay spanning
// the whole dial converts a raw tap point to polar coordinates and maps it
// to a wedge index directly, rather than fighting LVGL's per-object
// hit-test event for a wedge shape.
class PieMenu
{
public:
    // outerRadius/innerRadius in px, centered on parent's center. All
    // wedges/hub/overlay are created as direct children of parent -- there
    // is no single root object to return (parent already scopes lifetime,
    // e.g. deleting the LVGL screen that owns parent cleans everything up).
    void create(lv_obj_t *parent, const PieMenuItem *items, int count,
                lv_coord_t outerRadius, lv_coord_t innerRadius);

    void selectNext();
    void selectPrev();
    int selectedIndex() const { return selectedIndex_; }

    // Fired when the highlighted wedge is "opened" (second tap on it, or a
    // knob click while it's selected).
    void setOnOpen(void (*cb)(int index)) { onOpen_ = cb; }
    // Fired when the center hub is tapped.
    void setOnBack(void (*cb)()) { onBack_ = cb; }

    // The hub's main/hint lines ("Jog" / "press to open") are managed
    // automatically from the selected item's label on every selection
    // change. The title line (small, top) is caller-driven -- intended for
    // live machine status (mode text, color-coded) since the dial itself
    // has no notion of that.
    void setHubTitle(const char *title, lv_color_t color);

private:
    static const int MAX_ITEMS = 8;

    struct Wedge
    {
        lv_obj_t *bg;
        lv_obj_t *iconLbl;
        lv_obj_t *nameLbl;
        lv_coord_t startAngle; // LVGL convention: 0=right, clockwise
        lv_coord_t endAngle;

        // Mask state lives per-wedge (not shared/static) so each wedge's
        // draw-begin/end pair is self-contained regardless of draw order.
        lv_draw_mask_angle_param_t angleMaskParam;
        lv_draw_mask_radius_param_t holeMaskParam;
        int16_t angleMaskId = -1;
        int16_t holeMaskId = -1;
    };

    lv_obj_t *root_ = nullptr;
    lv_obj_t *overlay_ = nullptr;
    lv_obj_t *hub_ = nullptr;
    lv_obj_t *hubTitleLbl_ = nullptr;
    lv_obj_t *hubMainLbl_ = nullptr;
    lv_obj_t *hubHintLbl_ = nullptr;

    // Passed as event user_data so the static draw callbacks know both
    // which PieMenu instance and which wedge they're drawing.
    struct WedgeCbCtx
    {
        PieMenu *menu;
        int index;
    };

    Wedge wedges_[MAX_ITEMS];
    WedgeCbCtx wedgeCtx_[MAX_ITEMS];
    const char *labels_[MAX_ITEMS] = {nullptr};
    int itemCount_ = 0;
    int selectedIndex_ = 0;
    int styledSelectedIndex_ = -1; // last index restyleWedge() actually painted as selected
    lv_coord_t outerRadius_ = 0;
    lv_coord_t innerRadius_ = 0;
    lv_coord_t centerX_ = 0;
    lv_coord_t centerY_ = 0;

    void (*onOpen_)(int index) = nullptr;
    void (*onBack_)() = nullptr;

    void restyleSelection();
    void restyleWedge(int index, bool selected);
    void handleTap(lv_coord_t x, lv_coord_t y);

    static void overlayEventCb(lv_event_t *e);
    static void wedgeDrawBeginCb(lv_event_t *e);
    static void wedgeDrawEndCb(lv_event_t *e);
};
