#include "ui_dial.h"
#include "carousel.h"
#include "palette.h"

// Home: an 8-destination carousel (Jobs, Jog, Pen, Lights, Home, E-Stop,
// Alarm, Settings), replacing the old radial pie-menu ring -- see the
// "TerraPen Dial UI" mockup (Radial dial menu mockups/design_handoff_
// radial_dial_ui/), which switched every multi-item menu to this pattern
// once Home grew past what reads well as a ring. "Job Progress" isn't a
// direct destination here -- ui_nav auto-navigates to it when a job starts.
namespace
{
    struct DialItem
    {
        const char *label;
        const char *icon; // LV_SYMBOL_* placeholder standing in for real Lucide icons (not wired up yet)
    };

    const DialItem DIAL_ITEMS[] = {
        {"Jobs", LV_SYMBOL_FILE},
        {"Jog", LV_SYMBOL_GPS},
        {"Pen", LV_SYMBOL_EDIT},
        {"Lights", LV_SYMBOL_TINT},
        {"Home", LV_SYMBOL_HOME},
        {"E-Stop", LV_SYMBOL_STOP},
        {"Alarm", LV_SYMBOL_WARNING},
        {"Settings", LV_SYMBOL_SETTINGS},
    };
    const int DIAL_ITEM_COUNT = 8;

    Carousel carousel;
    lv_obj_t *statusLbl = nullptr;

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

    lv_color_t colorForMode(MachineMode mode)
    {
        switch (mode)
        {
            case MachineMode::Run:    return lv_color_hex(0xffe0b3);
            case MachineMode::Hold:   return lv_color_hex(0xff8200);
            case MachineMode::Alarm:  return Palette::accent();
            case MachineMode::Homing: return lv_color_hex(0x9c27b0);
            case MachineMode::Done:   return lv_color_hex(0x00ff3c);
            case MachineMode::Boot:   return lv_color_hex(0x4aa3ff);
            case MachineMode::Idle:
            default:                  return Palette::textMuted();
        }
    }

    lv_obj_t *makeCard(lv_obj_t *parent, const char *label, const char *icon)
    {
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_set_style_bg_color(card, Palette::bgTerminal(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 20, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_shadow_width(card, 16, 0);
        lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(card, 8, 0);

        lv_obj_t *iconLbl = lv_label_create(card);
        lv_label_set_text(iconLbl, icon);
        lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(iconLbl, Palette::accent(), 0);

        lv_obj_t *nameLbl = lv_label_create(card);
        lv_label_set_text(nameLbl, label);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nameLbl, lv_color_white(), 0);

        return card;
    }
}

lv_obj_t *uiDialCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    statusLbl = lv_label_create(scr);
    lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_12, 0);
    lv_obj_align(statusLbl, LV_ALIGN_TOP_MID, 0, 18);

    carousel.create(scr);
    for (int i = 0; i < DIAL_ITEM_COUNT; i++)
        carousel.addCard(makeCard(scr, DIAL_ITEMS[i].label, DIAL_ITEMS[i].icon));

    return scr;
}

void uiDialSetHandlers(void (*onOpen)(int index))
{
    carousel.setOnOpen(onOpen);
}

void uiDialSelectNext() { carousel.selectNext(); }
void uiDialSelectPrev() { carousel.selectPrev(); }
void uiDialOpenSelected() { carousel.openSelected(); }

void uiDialUpdate(const FluidNCStatus &st)
{
    lv_label_set_text(statusLbl, textForMode(st.mode));
    lv_obj_set_style_text_color(statusLbl, colorForMode(st.mode), 0);
}
