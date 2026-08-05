#include "ui_dial.h"
#include "pie_menu.h"
#include "palette.h" // found via the project's global -Iinclude build flag

namespace
{
    // Placeholder icons (LVGL built-in symbols) standing in for the real
    // Lucide glyphs (Crosshair/FileText/PenLine/Palette/House/CircleSlash2)
    // -- see pie_menu.cpp's header comment for why those aren't wired up
    // yet.
    const PieMenuItem DIAL_ITEMS[] = {
        {"Jog", LV_SYMBOL_GPS},
        {"Jobs", LV_SYMBOL_FILE},
        {"Pen", LV_SYMBOL_EDIT},
        {"Lights", LV_SYMBOL_TINT},
        {"Home", LV_SYMBOL_HOME},
        {"Stop", LV_SYMBOL_STOP},
    };
    const int DIAL_ITEM_COUNT = 6;

    PieMenu pieMenu;
    void (*openHandler)(int) = nullptr;

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
}

lv_obj_t *uiDialCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    pieMenu.create(scr, DIAL_ITEMS, DIAL_ITEM_COUNT, 118, 58);

    return scr;
}

void uiDialSetHandlers(void (*onOpen)(int index), void (*onBack)())
{
    openHandler = onOpen;
    pieMenu.setOnOpen(onOpen);
    pieMenu.setOnBack(onBack);
}

void uiDialSelectNext() { pieMenu.selectNext(); }
void uiDialSelectPrev() { pieMenu.selectPrev(); }

void uiDialOpenSelected()
{
    // Mirrors what a second tap on the already-selected wedge does -- a
    // knob click has no "which wedge" of its own, it just acts on whatever
    // is currently highlighted.
    if (openHandler) openHandler(pieMenu.selectedIndex());
}

void uiDialUpdate(const FluidNCStatus &st)
{
    pieMenu.setHubTitle(textForMode(st.mode), colorForMode(st.mode));
    // Tapping the hub while alarmed clears the alarm (see ui_nav's
    // onDialBack) instead of doing nothing -- surface that on the hint line
    // so it's discoverable rather than a hidden gesture.
    pieMenu.setHubHint(st.mode == MachineMode::Alarm ? "tap to clear" : "press to open");
}
