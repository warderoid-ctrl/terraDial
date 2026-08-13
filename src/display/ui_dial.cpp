#include "ui_dial.h"
#include "palette.h"
#include "radial_ring.h"
#include "icon_lightbulb.h"

// Home: a radial dial -- 8 destinations arranged on a ring around a centre
// hub, per the "TerraPen Dial UI" mockup. The item nearest the top slot is
// largest/brightest; others shrink and fade with angular distance. The ring
// mechanics live in RadialRing (shared with the Jobs screen); this file
// supplies the items, their colours, and the hub.
//
// "Job Progress" isn't a destination here -- ui_nav auto-navigates to it
// when a job starts.
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

    const DialItem DIAL_ITEMS[] = {
        {"Jobs", LV_SYMBOL_FILE, nullptr, false},
        {"Jog", LV_SYMBOL_GPS, nullptr, false},
        {"Pen", LV_SYMBOL_EDIT, nullptr, false},
        {"Lights", nullptr, &iconLightbulb, false}, // real Lucide bulb -- LVGL's symbol font has no lamp glyph
        {"Home XY", LV_SYMBOL_HOME, nullptr, false},
        {"E-Stop", LV_SYMBOL_STOP, nullptr, true},
        {"Alarm", LV_SYMBOL_WARNING, nullptr, false},
        {"Settings", LV_SYMBOL_SETTINGS, nullptr, false},
    };
    const int DIAL_ITEM_COUNT = 8;

    const lv_coord_t HUB_SIZE = 82; // holds the selected item's name + machine status

    RadialRing ring;
    lv_obj_t *statusLbl = nullptr;
    lv_obj_t *nameLbl = nullptr;
    lv_obj_t *iconObjs[DIAL_ITEM_COUNT] = {nullptr};
    bool iconIsLarge[DIAL_ITEM_COUNT] = {false};

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

        // Hysteresis, NOT a plain `nearness > 0.5` test. nearness 0.5 is
        // exactly 90 degrees -- the 3 and 9 o'clock slots, which are resting
        // positions for an 8-item ring. A single threshold there sits right
        // on the boundary, so float noise flipped the icon between its two
        // sizes every frame and made those two items visibly stutter.
        bool large = iconIsLarge[i] ? (nearness > 0.42f) : (nearness > 0.58f);

        if (DIAL_ITEMS[i].iconImg)
        {
            // Alpha bitmaps take their colour from img_recolor rather than
            // text_color. Size is deliberately left alone: scaling via
            // lv_img_set_zoom sent LVGL down its transform path, which
            // combined with the parent card's opacity < 255 made the icon
            // intermittently vanish altogether. The bitmap is generated at
            // a size that reads correctly drawn 1:1 (tools/gen_lucide_icon.py).
            lv_obj_set_style_img_recolor(iconObjs[i], iconColor, 0);
            iconIsLarge[i] = large; // tracked only so the font path below stays in sync
        }
        else
        {
            lv_obj_set_style_text_color(iconObjs[i], iconColor, 0);
            // A font change forces a label relayout, unlike the plain
            // colour/size writes above -- skip it unless the near/far bucket
            // actually flipped, since this runs for every item on every
            // animation frame.
            if (large != iconIsLarge[i])
            {
                iconIsLarge[i] = large;
                lv_obj_set_style_text_font(iconObjs[i], large ? &lv_font_montserrat_24 : &lv_font_montserrat_14, 0);
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
            lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_24, 0);
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

    ring.create(scr);
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
