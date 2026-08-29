#include "ui_dial.h"
#include "palette.h"
#include "radial_ring.h"
#include "icon_lightbulb.h"
#include "ui_widgets.h"

// Home: a radial dial -- 10 destinations arranged on a ring around a centre
// hub, per the "TerraPen Dial UI" mockup. The item nearest the top slot is
// largest/brightest; others shrink and fade with angular distance. The ring
// mechanics live in RadialRing (shared with the Jobs screen); this file
// supplies the items, their colours, and the hub.
//
// The spacing is deliberately uneven (RadialRing::setSpread): the top of the
// ring is stretched open and the bottom squeezed shut. Testers liked the
// dial but couldn't read it -- equal chips a uniform 360/count degrees apart
// on a 1.28" panel all look much the same. Spending the angular budget where
// you're looking lets the selection and its two neighbours grow enough to
// tell apart, at the cost of the items you're rotating away from.
//
// That budget is what shrinks as items are added: the ring is a full circle,
// so the even spacing behind the spread is 360/count -- 40 degrees at nine
// items, 36 at ten. The spread absorbs it (the tail bunches tighter while the
// selection keeps its room), but this is the cost of a new dial item, and it
// is why one earns its place rather than being added because it fits.
//
// Job Progress is both auto-navigated to when a job starts AND a dial item
// in its own right, because those solve different problems: the auto-nav
// shows you the job without asking, the dial item gets you back to it after
// you've wandered off to change the lights mid-run. Without the item, the
// only route back was to wait for the job to end.
namespace
{
    struct DialItem
    {
        const char *label;
        const char *icon; // LV_SYMBOL_* placeholder standing in for a real Lucide icon
        // Set instead of `icon` to use a real Lucide glyph rasterised to an
        // alpha bitmap (see icon_lightbulb.h). Alpha-only, so it recolours
        // with the ring exactly like the symbol-font icons do.
        const lv_img_dsc_t *iconImg;
        // Pinned to alert red regardless of ring position, instead of
        // fading between the raised surface and the accent like every other
        // item. E-Stop has to be findable at a glance mid-panic -- if it
        // only turned red once rotated to the top, you'd be hunting for it
        // exactly when you can least afford to.
        bool alwaysAlert;
    };

    // Ordered by how a session actually runs, not by category: you home,
    // you jog to the work, you set the pen, then you pick a job. The ring
    // rests on item 0, so the first thing under the selection when the
    // dial comes up is the first thing you do. Setup-and-go items lead;
    // the two you reach for when something is wrong (E-Stop, Alarm) sit
    // mid-ring where the alert-red chip is easy to find, and the two you
    // rarely touch mid-job trail at the end.
    const DialItem DIAL_ITEMS[] = {
        {"Home XY", LV_SYMBOL_HOME, nullptr, false},
        {"Jog", LV_SYMBOL_GPS, nullptr, false},
        {"Pen", LV_SYMBOL_EDIT, nullptr, false},
        {"Jobs", LV_SYMBOL_FILE, nullptr, false},
        {"Progress", LV_SYMBOL_LOOP, nullptr, false},
        // Directly after Progress because that is where it falls in a
        // session: you watch the job finish, then you park to photograph it.
        {"Photo", LV_SYMBOL_IMAGE, nullptr, false},
        {"E-Stop", LV_SYMBOL_STOP, nullptr, true},
        {"Alarm", LV_SYMBOL_WARNING, nullptr, false},
        {"Lights", nullptr, &iconLightbulb, false}, // real Lucide bulb -- LVGL's symbol font has no lamp glyph
        {"Settings", LV_SYMBOL_SETTINGS, nullptr, false},
    };
    const int DIAL_ITEM_COUNT = 10;

    const lv_coord_t HUB_SIZE = 82; // holds the selected item's name + machine status

    // Ring geometry. Wider, and with a much bigger near/far ratio than the
    // RadialRing defaults -- which is what the spread buys: at rest the
    // selected chip is 66px and the bunched pair at the bottom are 24-29px,
    // so "which one is selected" is answered by size alone from arm's
    // length.
    //
    // The near size is capped by the hub, not by the panel: 66 at radius 76
    // leaves the selected chip 2px clear of the hub's rim and 31px inside
    // the panel edge. Grow it and the top chip starts covering the hub's
    // name label.
    const lv_coord_t RING_RADIUS = 76;
    const lv_coord_t RING_SIZE_NEAR = 66;
    const lv_coord_t RING_SIZE_FAR = 24;
    const lv_opa_t RING_OPA_FAR = 100;
    const float RING_SPREAD = 0.55f;

    RadialRing ring;
    lv_obj_t *statusLbl = nullptr;
    lv_obj_t *nameLbl = nullptr;
    lv_obj_t *iconObjs[DIAL_ITEM_COUNT] = {nullptr};

    // Which icon size each item is currently drawn at -- see uiRingIconSize.
    UiRingIconSize iconSize[DIAL_ITEM_COUNT] = {UiRingIconSmall};

    void updateNameLabel()
    {
        lv_label_set_text(nameLbl, DIAL_ITEMS[ring.selectedIndex()].label);
    }

    void onSelect(int) { updateNameLabel(); }

    void onItemStyle(lv_obj_t *card, int i, float nearness)
    {
        // Card fades from the raised navy surface up to the red accent as it
        // approaches the top slot; its icon fades from muted to full white
        // so the selected item is unmistakable. E-Stop opts out of both the
        // colour blend and the distance fade -- a dimmed E-Stop would defeat
        // the point of pinning its colour.
        lv_opa_t mix = (lv_opa_t)(255 * nearness);
        bool alert = DIAL_ITEMS[i].alwaysAlert;

        if (alert) lv_obj_set_style_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(card,
                                  alert ? Palette::alert()
                                        : lv_color_mix(Palette::accent(), Palette::bgSecondary(), mix),
                                  0);

        lv_color_t iconColor = alert ? Palette::accentFg()
                                     : lv_color_mix(Palette::accentFg(), Palette::textMuted(), mix);

        UiRingIconSize want = uiRingIconSize(nearness, iconSize[i]);

        if (DIAL_ITEMS[i].iconImg)
        {
            // Alpha bitmaps take their colour from img_recolor rather than
            // text_color.
            lv_obj_set_style_img_recolor(iconObjs[i], iconColor, 0);

            // Two pre-rasterised bitmaps rather than lv_img_set_zoom:
            // scaling sends LVGL down its transform path, which combined
            // with the parent card's opacity < 255 made the icon
            // intermittently vanish altogether. Only the top slot gets the
            // big one -- there's no third bitmap, so medium shares the
            // small one, which is fine since medium chips are the size the
            // 20px bulb was drawn for.
            if (want != iconSize[i])
            {
                iconSize[i] = want;
                lv_img_set_src(iconObjs[i], want == UiRingIconLarge ? &iconLightbulbLarge : &iconLightbulb);
            }
        }
        else
        {
            lv_obj_set_style_text_color(iconObjs[i], iconColor, 0);
            // A font change forces a label relayout, unlike the plain
            // colour write above -- skip it unless the size bucket actually
            // flipped, since this runs for every item on every animation
            // frame.
            if (want != iconSize[i])
            {
                iconSize[i] = want;
                lv_obj_set_style_text_font(iconObjs[i], uiRingIconFont(want), 0);
            }
        }
    }

    lv_obj_t *makeCard(lv_obj_t *parent, int index)
    {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_shadow_width(card, 12, 0);
        lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(card, 0, 0);
        // Extends the touch area beyond the drawn circle without changing
        // how it looks -- the shrunken off-top cards are much smaller than a
        // fingertip.
        lv_obj_set_ext_click_area(card, 10);

        if (DIAL_ITEMS[index].iconImg)
        {
            lv_obj_t *img = lv_img_create(card);
            lv_img_set_src(img, DIAL_ITEMS[index].iconImg);
            // Alpha-only source: recolor_opa must be on or it draws nothing.
            lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
            lv_obj_center(img);
            iconObjs[index] = img;
        }
        else
        {
            lv_obj_t *iconLbl = lv_label_create(card);
            lv_label_set_text(iconLbl, DIAL_ITEMS[index].icon);
            // Must match iconSize[]'s initial value: onItemStyle only writes
            // a font when the bucket CHANGES, so a mismatch here would leave
            // far items drawn at the wrong size until they happened to pass
            // through another bucket.
            lv_obj_set_style_text_font(iconLbl, uiRingIconFont(UiRingIconSmall), 0);
            lv_obj_center(iconLbl);
            iconObjs[index] = iconLbl;
        }
        return card;
    }

    void hubTapCb(lv_event_t *e)
    {
        (void)e;
        ring.openSelected();
    }

    const char *textForMode(MachineMode mode)
    {
        switch (mode)
        {
            case MachineMode::Run:    return "RUN";
            case MachineMode::Hold:   return "HOLD";
            case MachineMode::Alarm:  return "ALARM";
            case MachineMode::Homing: return "HOMING";
            case MachineMode::Done:   return "DONE";
            case MachineMode::Boot:   return "CONNECTING";
            case MachineMode::Idle:
            default:                  return "IDLE";
        }
    }

    // Machine-state colours, pulled toward the terraForge palette so the hub
    // doesn't read as a set of unrelated primaries against the navy. Run/
    // Done stay green-ish and Hold stays amber because those meanings are
    // near-universal on CNC gear and worth keeping literal.
    lv_color_t colorForMode(MachineMode mode)
    {
        switch (mode)
        {
            case MachineMode::Run:    return lv_color_hex(0x3ddc84);
            case MachineMode::Hold:   return lv_color_hex(0xffa726);
            case MachineMode::Alarm:  return Palette::accent();          // the terraForge red
            case MachineMode::Homing: return Palette::accentSecondary(); // file-browser blue
            case MachineMode::Done:   return lv_color_hex(0x3ddc84);
            case MachineMode::Boot:   return Palette::textFaint();
            case MachineMode::Idle:
            default:                  return Palette::textMuted();
        }
    }
}

lv_obj_t *uiDialCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    // Centre hub -- same circular styling as the ring items, just bigger and
    // stationary. Holds the selected item's name (so rotating never hides
    // it behind the top card) and the live machine status.
    lv_obj_t *hub = lv_obj_create(scr);
    lv_obj_set_size(hub, HUB_SIZE, HUB_SIZE);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, Palette::bgSecondary(), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(hub, Palette::border(), 0);
    lv_obj_set_style_border_width(hub, 1, 0);
    lv_obj_set_style_pad_all(hub, 0, 0);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    // The hub names the selected item, so tapping it opens that item -- a
    // big, central, always-in-the-same-place target, unlike the ring cards
    // which move as you rotate.
    lv_obj_add_flag(hub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hub, hubTapCb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);

    nameLbl = lv_label_create(hub);
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(nameLbl, Palette::text(), 0);
    lv_obj_align(nameLbl, LV_ALIGN_CENTER, 0, -8);

    statusLbl = lv_label_create(hub);
    lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_12, 0);
    lv_obj_align(statusLbl, LV_ALIGN_CENTER, 0, 12);

    ring.create(scr, RING_RADIUS, RING_SIZE_NEAR, RING_SIZE_FAR, LV_OPA_COVER, RING_OPA_FAR);
    ring.setSpread(RING_SPREAD);
    ring.setOnItemStyle(onItemStyle);
    ring.setOnSelect(onSelect);
    for (int i = 0; i < DIAL_ITEM_COUNT; i++) ring.addItem(makeCard(scr, i));

    // Items are created after the hub, so raise it back above them.
    lv_obj_move_foreground(hub);

    updateNameLabel();
    return scr;
}

void uiDialSetHandlers(void (*onOpen)(int index))
{
    ring.setOnOpen(onOpen);
}

void uiDialSelectNext() { ring.selectNext(); }
void uiDialSelectPrev() { ring.selectPrev(); }
void uiDialOpenSelected() { ring.openSelected(); }

void uiDialUpdate(const FluidNCStatus &st)
{
    lv_label_set_text(statusLbl, textForMode(st.mode));
    lv_obj_set_style_text_color(statusLbl, colorForMode(st.mode), 0);
}
