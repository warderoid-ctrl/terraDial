#include "ui_files.h"
#include "../net/fluidnc_client.h"
#include "radial_ring.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <stdio.h>
#include <string.h>

// "Jobs" -- SD-card file browsing, presented as the same radial ring the
// home dial uses (RadialRing), with the selected job's name in the centre
// hub.
//
// It was a carousel of detail cards before. The problem wasn't the paging,
// it was that the peeking cards rendered a filename and size at ~84px and
// half-faded: text you couldn't read but your eye kept trying to. Ring
// chips carry no text at all, so there's nothing to squint at -- you rotate
// and read the one place that matters, the hub. It also means Jobs and Home
// now share one browsing idiom instead of two.
namespace
{
    RadialRing ring;
    lv_obj_t *screenRoot = nullptr;
    lv_obj_t *hubNameLbl = nullptr;
    lv_obj_t *hubMetaLbl = nullptr;
    lv_obj_t *hubActionLbl = nullptr;
    char selectedFile[48] = {0};

    void formatSize(char *buf, size_t bufSize, int32_t size)
    {
        if (size >= 1024 * 1024) snprintf(buf, bufSize, "%.1f MB", size / 1048576.0f);
        else if (size >= 1024) snprintf(buf, bufSize, "%.1f KB", size / 1024.0f);
        else snprintf(buf, bufSize, "%ld B", (long)size);
    }

    void showEmptyHub()
    {
        lv_label_set_text(hubNameLbl, "No jobs");
        lv_label_set_text(hubMetaLbl, "SD card empty");
        lv_label_set_text(hubActionLbl, "");
    }

    void refreshHub(int index)
    {
        FluidNCFileEntry entry;
        if (!fluidNC.fileListEntry(index, entry))
        {
            showEmptyHub();
            return;
        }
        // Long filenames are the normal case, not an edge case -- the label
        // is width-limited and ellipsises rather than wrapping the hub open.
        lv_label_set_text(hubNameLbl, entry.name);
        char sizeBuf[16];
        formatSize(sizeBuf, sizeof(sizeBuf), entry.size);
        lv_label_set_text(hubMetaLbl, sizeBuf);
        lv_label_set_text(hubActionLbl, LV_SYMBOL_PLAY " Run");
    }

    void confirmCb(lv_event_t *e)
    {
        lv_obj_t *mbox = lv_event_get_current_target(e);
        const char *txt = lv_msgbox_get_active_btn_text(mbox);
        if (txt && !strcmp(txt, "Run")) fluidNC.runFile(selectedFile);
        else if (txt && !strcmp(txt, "Delete")) fluidNC.deleteFile(selectedFile);
        lv_msgbox_close(mbox);
    }

    void openConfirmFor(const char *filename)
    {
        strncpy(selectedFile, filename, sizeof(selectedFile) - 1);
        selectedFile[sizeof(selectedFile) - 1] = '\0';

        static const char *btns[] = {"Run", "Delete", "Cancel", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "File", selectedFile, btns, false);
        lv_obj_center(mbox);
        lv_obj_add_event_cb(mbox, confirmCb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // Same colour treatment as the dial's items, so the two rings read as
    // one system: the chip warms from the raised navy surface to the red
    // accent as it reaches the top slot.
    void onItemStyle(lv_obj_t *chip, int, float nearness)
    {
        lv_opa_t mix = (lv_opa_t)(255 * nearness);
        lv_obj_set_style_bg_color(chip, lv_color_mix(Palette::accent(), Palette::bgSecondary(), mix), 0);

        lv_obj_t *icon = lv_obj_get_child(chip, 0);
        if (icon)
            lv_obj_set_style_text_color(icon, lv_color_mix(Palette::accentFg(), Palette::textMuted(), mix), 0);
    }

    lv_obj_t *makeChip()
    {
        lv_obj_t *chip = lv_obj_create(screenRoot);
        lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(chip, 0, 0);
        lv_obj_set_style_shadow_width(chip, 12, 0);
        lv_obj_set_style_shadow_color(chip, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(chip, LV_OPA_30, 0);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(chip, 0, 0);
        lv_obj_set_ext_click_area(chip, 10);

        lv_obj_t *icon = lv_label_create(chip);
        lv_label_set_text(icon, LV_SYMBOL_FILE);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
        lv_obj_center(icon);
        return chip;
    }

    void rebuildList()
    {
        ring.clear();
        int count = fluidNC.fileListCount();
        for (int i = 0; i < count && i < RadialRing::MAX_ITEMS; i++) ring.addItem(makeChip());

        if (count == 0) showEmptyHub();
        else refreshHub(ring.selectedIndex());
    }

    void onCardOpen(int index)
    {
        FluidNCFileEntry entry;
        if (!fluidNC.fileListEntry(index, entry)) return;
        fluidNC.runFile(entry.name);
    }

    void hubTapCb(lv_event_t *e)
    {
        (void)e;
        ring.openSelected();
    }
}

lv_obj_t *uiFilesCreate()
{
    screenRoot = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screenRoot, Palette::bgApp(), 0);

    // Centre hub: the one place a filename is readable, and the run button.
    // Slightly larger than the dial's hub because filenames need the width.
    lv_obj_t *hub = lv_obj_create(screenRoot);
    lv_obj_set_size(hub, 96, 96);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, Palette::bgSecondary(), 0);
    lv_obj_set_style_bg_color(hub, Palette::accentHover(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(hub, Palette::border(), 0);
    lv_obj_set_style_border_width(hub, 1, 0);
    lv_obj_set_style_pad_all(hub, 0, 0);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hub, hubTapCb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);

    hubNameLbl = lv_label_create(hub);
    lv_obj_set_width(hubNameLbl, 82);
    lv_label_set_long_mode(hubNameLbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hubNameLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(hubNameLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hubNameLbl, Palette::text(), 0);
    lv_obj_align(hubNameLbl, LV_ALIGN_CENTER, 0, -18);

    hubMetaLbl = lv_label_create(hub);
    lv_obj_set_style_text_font(hubMetaLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hubMetaLbl, Palette::textMuted(), 0);
    lv_obj_align(hubMetaLbl, LV_ALIGN_CENTER, 0, 4);

    hubActionLbl = lv_label_create(hub);
    lv_obj_set_style_text_font(hubActionLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hubActionLbl, Palette::accent(), 0);
    lv_obj_align(hubActionLbl, LV_ALIGN_CENTER, 0, 24);

    // Arc layout rather than a full circle: 30-degree pitch, and nothing
    // drawn past +/-132 degrees. That empties the 5/6/7 o'clock arc so the
    // back button below can't be mistaken for a job chip -- mis-tapping one
    // starts a plot, which is not a cheap mistake -- and it lets the list
    // scroll instead of squeezing every file onto one circle.
    // opaFar 0 makes chips fade right out as they reach the arc edge.
    ring.create(screenRoot, 74, 56, 26, LV_OPA_COVER, LV_OPA_TRANSP);
    ring.setArcLayout(30.0f, 132.0f);
    ring.setOnOpen(onCardOpen);
    ring.setOnItemStyle(onItemStyle);
    ring.setOnSelect(refreshHub);

    lv_obj_move_foreground(hub);
    showEmptyHub();

    addBackButton(screenRoot);
    return screenRoot;
}

void uiFilesSetFocused(bool focused)
{
    if (focused) fluidNC.requestFileList();
}

void uiFilesHandleRotate(int32_t delta)
{
    if (delta == 0) return;
    for (int32_t i = 0; i < delta; i++) ring.selectNext();
    for (int32_t i = 0; i < -delta; i++) ring.selectPrev();
}

void uiFilesHandleSelect()
{
    ring.openSelected();
}

void uiFilesHandleDoubleClick()
{
    FluidNCFileEntry entry;
    if (!fluidNC.fileListEntry(ring.selectedIndex(), entry)) return;
    openConfirmFor(entry.name);
}

void uiFilesUpdate()
{
    if (!fluidNC.fileListReady()) return;
    fluidNC.clearFileListReady();
    rebuildList();
}
