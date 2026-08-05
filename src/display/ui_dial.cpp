#include "ui_dial.h"
#include "palette.h"
#include "icon_lightbulb.h"
#include <math.h>

// Home: a radial dial -- 8 destinations (Jobs, Jog, Pen, Lights, Home,
// E-Stop, Alarm, Settings) arranged on a ring around the center, per the
// "TerraPen Dial UI" mockup (Radial dial menu mockups/design_handoff_
// radial_dial_ui/), which asks for Home specifically to revert from the
// carousel pattern used elsewhere (Jobs/Settings keep the carousel -- see
// carousel.h/ui_files.cpp/ui_settings.cpp, unchanged by this file).
// The item nearest the top slot is largest/brightest; others shrink and
// fade continuously with angular distance from top. Rotating the knob
// spins the whole ring with a springy overshoot-eased transition rather
// than a flat swap. "Job Progress" isn't a direct destination here --
// ui_nav auto-navigates to it when a job starts.
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
    const float STEP_DEG = 360.0f / DIAL_ITEM_COUNT;

    // Ring geometry, tuned for the real 240x240 round panel (visible
    // radius ~120px) -- keep the largest card's reach under that or it
    // clips the bezel on the top item.
    const lv_coord_t RING_RADIUS = 74;
    const lv_coord_t HUB_SIZE = 82; // center hub circle -- holds the selected item's name + machine status

    // Continuous shrink/fade/color-blend range by angular distance (0 =
    // dead center-top, 180 = directly opposite). Real size changes, not
    // style transform_zoom -- combining transform_zoom with opa<255 forces
    // LVGL to render each card through an offscreen layer every frame,
    // which was unreliable on this device's heap (cards render blank at
    // rest, flash briefly mid-animation). Plain resize+opa is the same
    // technique the Jobs/Settings carousel already uses without issue.
    const lv_coord_t SIZE_NEAR = 62;
    const lv_coord_t SIZE_FAR = 30;
    const lv_opa_t OPA_NEAR = LV_OPA_COVER;
    const lv_opa_t OPA_FAR = 110;

    lv_obj_t *statusLbl = nullptr;
    lv_obj_t *nameLbl = nullptr;
    lv_obj_t *cards[DIAL_ITEM_COUNT];
    lv_obj_t *iconLbls[DIAL_ITEM_COUNT];
    bool iconIsLarge[DIAL_ITEM_COUNT] = {false};

    int selectedIndex = 0;
    int32_t offsetX100 = 0;       // ring rotation as currently rendered (moves mid-animation)
    int32_t targetOffsetX100 = 0; // last commanded rest position -- always an exact multiple of
                                   // STEP_DEG*100 relative to 0. New targets are always computed
                                   // from THIS, never from offsetX100: offsetX100 can be caught
                                   // mid-animation (e.g. spinning the knob fast enough to interrupt
                                   // an in-flight spring before it settles), and computing the next
                                   // target from an unsettled value permanently knocks the ring's
                                   // rest position off-grid from selectedIndex -- exactly the "top
                                   // item stops matching what opens" bug this avoids.
    void (*onOpenCb)(int index) = nullptr;

    float normalizeAngle(float deg)
    {
        while (deg > 180.0f) deg -= 360.0f;
        while (deg < -180.0f) deg += 360.0f;
        return deg;
    }

    void layoutRing()
    {
        float offsetDeg = offsetX100 / 100.0f;
        for (int i = 0; i < DIAL_ITEM_COUNT; i++)
        {
            float angle = normalizeAngle(i * STEP_DEG + offsetDeg);
            float dist = fabsf(angle) / 180.0f; // 0..1
            float near = 1.0f - dist;

            float rad = angle * (float)M_PI / 180.0f;
            lv_coord_t x = (lv_coord_t)(RING_RADIUS * sinf(rad));
            lv_coord_t y = (lv_coord_t)(-RING_RADIUS * cosf(rad));

            lv_coord_t size = SIZE_FAR + (lv_coord_t)((SIZE_NEAR - SIZE_FAR) * near);
            lv_obj_set_size(cards[i], size, size);
            lv_obj_align(cards[i], LV_ALIGN_CENTER, x, y);

            // alwaysAlert items skip the distance fade too -- a dimmed
            // E-Stop would defeat the point of pinning its colour.
            lv_opa_t opa = DIAL_ITEMS[i].alwaysAlert
                               ? LV_OPA_COVER
                               : (lv_opa_t)(OPA_FAR + (lv_opa_t)((OPA_NEAR - OPA_FAR) * near));
            lv_obj_set_style_opa(cards[i], opa, 0);

            // Card fades from the raised navy surface up to the red accent as
            // it approaches the top slot; its icon fades from muted to full
            // white so the selected item is unmistakable. E-Stop opts out --
            // it stays alert red at every position.
            lv_opa_t mix = (lv_opa_t)(255 * near);
            lv_color_t iconColor = DIAL_ITEMS[i].alwaysAlert
                                       ? Palette::accentFg()
                                       : lv_color_mix(Palette::accentFg(), Palette::textMuted(), mix);
            lv_obj_set_style_bg_color(cards[i],
                                      DIAL_ITEMS[i].alwaysAlert
                                          ? Palette::alert()
                                          : lv_color_mix(Palette::accent(), Palette::bgSecondary(), mix),
                                      0);

            bool large = near > 0.5f;
            if (DIAL_ITEMS[i].iconImg)
            {
                // Alpha bitmaps take their colour from img_recolor, not
                // text_color, and scale via the image's own zoom (256 =
                // 1:1) rather than a font swap.
                lv_obj_set_style_img_recolor(iconLbls[i], iconColor, 0);
                if (large != iconIsLarge[i])
                {
                    iconIsLarge[i] = large;
                    lv_img_set_zoom(iconLbls[i], large ? 256 : 150);
                }
            }
            else
            {
                lv_obj_set_style_text_color(iconLbls[i], iconColor, 0);
                // lv_obj_set_style_text_font forces a relayout of the label
                // (font metrics changed), unlike the plain color/opa/size
                // writes above -- skip it unless the near/far bucket actually
                // flipped, since this runs on all 8 cards every animation
                // frame and was adding up to visible per-frame cost.
                if (large != iconIsLarge[i])
                {
                    iconIsLarge[i] = large;
                    lv_obj_set_style_text_font(iconLbls[i], large ? &lv_font_montserrat_24 : &lv_font_montserrat_14, 0);
                }
            }

            // Selected item stays foreground so it never renders under a
            // neighbor mid-rotation.
            if (i == selectedIndex) lv_obj_move_foreground(cards[i]);
        }
    }

    void ringAnimCb(void *, int32_t v)
    {
        offsetX100 = v;
        layoutRing();
    }

    // targetX100 becomes the new authoritative rest position; the anim just
    // eases the on-screen offsetX100 there from wherever it currently is.
    void animateTo(int32_t targetX100)
    {
        targetOffsetX100 = targetX100;
        lv_anim_del(&offsetX100, ringAnimCb);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, &offsetX100);
        lv_anim_set_exec_cb(&a, ringAnimCb);
        lv_anim_set_values(&a, offsetX100, targetX100);
        lv_anim_set_time(&a, 320); // was 550 -- felt laggy behind the physical knob
        lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
        lv_anim_start(&a);
    }

    void updateNameLabel()
    {
        lv_label_set_text(nameLbl, DIAL_ITEMS[selectedIndex].label);
    }

    void hubTapCb(lv_event_t *e)
    {
        (void)e;
        if (onOpenCb) onOpenCb(selectedIndex);
    }

    void cardTapCb(lv_event_t *e)
    {
        int i = (int)(intptr_t)lv_event_get_user_data(e);
        if (i != selectedIndex)
        {
            // Spin the tapped card to the top (shortest direction) so the
            // ring reads correctly if the user comes back to Home --
            // doesn't block opening below, which happens immediately.
            int fwd = (i - selectedIndex + DIAL_ITEM_COUNT) % DIAL_ITEM_COUNT;
            int back = DIAL_ITEM_COUNT - fwd;
            int32_t stepsX100 = (fwd <= back) ? (int32_t)(-fwd * STEP_DEG * 100) : (int32_t)(back * STEP_DEG * 100);
            selectedIndex = i;
            animateTo(targetOffsetX100 + stepsX100);
            updateNameLabel();
        }
        // Touch is a direct-select: tapping any card (top or a peeking
        // neighbor) opens it immediately, same as a knob click on the top
        // item -- touch doesn't need the two-step "spin into place, then
        // tap/click again to open" the knob's rotate-then-click implies.
        if (onOpenCb) onOpenCb(i);
    }

    lv_obj_t *makeCard(lv_obj_t *parent, int index, const char *label, const char *icon)
    {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_size(card, SIZE_NEAR, SIZE_NEAR); // layoutRing() resizes every frame; this is just the initial value
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_shadow_width(card, 12, 0);
        lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(card, 0, 0);
        // Extends the touch-sensitive area beyond the drawn circle without
        // changing how it looks -- the shrunken off-top cards are small
        // targets on a 240px panel, and a fingertip is much bigger than the
        // ~30px they shrink to.
        lv_obj_set_ext_click_area(card, 10);
        lv_obj_add_event_cb(card, cardTapCb, LV_EVENT_CLICKED, (void *)(intptr_t)index);

        if (DIAL_ITEMS[index].iconImg)
        {
            lv_obj_t *img = lv_img_create(card);
            lv_img_set_src(img, DIAL_ITEMS[index].iconImg);
            // Alpha-only source: recolor_opa must be on or it draws as
            // nothing. layoutRing() then sets the colour per frame.
            lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
            lv_obj_center(img);
            iconLbls[index] = img;
        }
        else
        {
            lv_obj_t *iconLbl = lv_label_create(card);
            lv_label_set_text(iconLbl, icon);
            lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_24, 0);
            lv_obj_center(iconLbl);
            iconLbls[index] = iconLbl;
        }

        (void)label; // name shown centrally via nameLbl, not per-card
        return card;
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

    // Center hub -- same circular-chip styling as the ring items, just
    // bigger and stationary. Holds the selected item's name (so rotating
    // the ring doesn't hide it behind the top card) and the live machine
    // status underneath it.
    lv_obj_t *hub = lv_obj_create(scr);
    lv_obj_set_size(hub, HUB_SIZE, HUB_SIZE);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, Palette::bgSecondary(), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(hub, Palette::border(), 0);
    lv_obj_set_style_border_width(hub, 1, 0);
    lv_obj_set_style_pad_all(hub, 0, 0);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    // The hub already names the selected item, so tapping it opens that item
    // -- a big, central, always-in-the-same-place target, unlike the ring
    // cards which move as you rotate.
    lv_obj_add_flag(hub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hub, hubTapCb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);

    nameLbl = lv_label_create(hub);
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(nameLbl, lv_color_white(), 0);
    lv_obj_align(nameLbl, LV_ALIGN_CENTER, 0, -8);

    statusLbl = lv_label_create(hub);
    lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_12, 0);
    lv_obj_align(statusLbl, LV_ALIGN_CENTER, 0, 12);

    for (int i = 0; i < DIAL_ITEM_COUNT; i++)
        cards[i] = makeCard(scr, i, DIAL_ITEMS[i].label, DIAL_ITEMS[i].icon);

    selectedIndex = 0;
    offsetX100 = 0;
    targetOffsetX100 = 0;
    layoutRing();
    updateNameLabel();

    return scr;
}

void uiDialSetHandlers(void (*onOpen)(int index))
{
    onOpenCb = onOpen;
}

void uiDialSelectNext()
{
    selectedIndex = (selectedIndex + 1) % DIAL_ITEM_COUNT;
    animateTo(targetOffsetX100 - (int32_t)(STEP_DEG * 100));
    updateNameLabel();
}

void uiDialSelectPrev()
{
    selectedIndex = (selectedIndex - 1 + DIAL_ITEM_COUNT) % DIAL_ITEM_COUNT;
    animateTo(targetOffsetX100 + (int32_t)(STEP_DEG * 100));
    updateNameLabel();
}

void uiDialOpenSelected()
{
    if (onOpenCb) onOpenCb(selectedIndex);
}

void uiDialUpdate(const FluidNCStatus &st)
{
    lv_label_set_text(statusLbl, textForMode(st.mode));
    lv_obj_set_style_text_color(statusLbl, colorForMode(st.mode), 0);
}
