#include "ui_files.h"
#include "../net/fluidnc_client.h"
#include "radial_ring.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include "ui_widgets.h"
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
    // Ring geometry. The arc is spread toward the top slot (see
    // RadialRing::setSpread) exactly like the home dial: on a 1.28" panel a
    // row of near-identical chips tells you nothing about which one the hub
    // is describing, so the selection gets the angular room to be visibly
    // the biggest thing on screen and the tail bunches up behind it.
    //
    // 60 at radius 80 leaves the selected chip 2px clear of the 96px hub,
    // which is what caps the near size here.
    const lv_coord_t RING_RADIUS = 80;
    const lv_coord_t RING_SIZE_NEAR = 60;
    const lv_coord_t RING_SIZE_FAR = 20;
    const float RING_SPREAD = 0.55f;

    RadialRing ring;
    lv_obj_t *screenRoot = nullptr;

    // Which icon size each chip is drawn at -- see uiRingIconSize. Reset
    // whenever the list is rebuilt, since the chips are destroyed with it.
    UiRingIconSize iconSize[RadialRing::MAX_ITEMS] = {UiRingIconSmall};
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
        // is width-limited and scrolls rather than wrapping the hub open.
        // Safe to set unconditionally: RadialRing only calls onSelect when
        // the selection actually changes, so this can't restart the scroll
        // animation mid-cycle the way a periodic update would.
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
    void onItemStyle(lv_obj_t *chip, int i, float nearness)
    {
        lv_opa_t mix = (lv_opa_t)(255 * nearness);
        lv_obj_set_style_bg_color(chip, lv_color_mix(Palette::accent(), Palette::bgSecondary(), mix), 0);

        lv_obj_t *icon = lv_obj_get_child(chip, 0);
        if (!icon) return;
        lv_obj_set_style_text_color(icon, lv_color_mix(Palette::accentFg(), Palette::textMuted(), mix), 0);

        // A font change forces a label relayout, unlike the colour write
        // above -- skip it unless the size bucket actually flipped, since
        // this runs for every chip on every animation frame.
        UiRingIconSize want = uiRingIconSize(nearness, iconSize[i]);
        if (want != iconSize[i])
        {
            iconSize[i] = want;
            lv_obj_set_style_text_font(icon, uiRingIconFont(want), 0);
        }
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
        // Must match iconSize[]'s reset value below -- onItemStyle only
        // writes a font when the bucket CHANGES, so a mismatch here leaves
        // chips drawn at the wrong size until they happen to cross a band.
        lv_obj_set_style_text_font(icon, uiRingIconFont(UiRingIconSmall), 0);
        lv_obj_center(icon);
        return chip;
    }

    void rebuildList()
    {
        ring.clear();
        for (int i = 0; i < RadialRing::MAX_ITEMS; i++) iconSize[i] = UiRingIconSmall;
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
    // Scrolls, rather than ellipsising. 82px holds roughly a dozen characters
    // and plotter files are routinely named by layer -- "drawing 1", "drawing
    // 2", "drawing 3" -- so the digit that tells them apart is the character
    // an ellipsis eats first. Every name here is a name you're choosing
    // between, so the end of it has to arrive on its own.
    //
    // CIRCULAR (wraps around through a gap) rather than plain SCROLL (runs to the
    // end, then reverses): the reversal reads as the text having stopped, and
    // on a name that only just overflows it can look like a twitch. LVGL only
    // animates when the text actually overflows, so short names sit still.
    lv_label_set_long_mode(hubNameLbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
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
    //
    // The pitch stays 30 degrees; the spread is what redistributes it, so
    // the chips either side of the selection sit ~45 degrees out and the
    // tail past them closes up toward the arc edge where it's fading out
    // anyway. Same number of files on screen, far more legible ordering.
    ring.create(screenRoot, RING_RADIUS, RING_SIZE_NEAR, RING_SIZE_FAR, LV_OPA_COVER, LV_OPA_TRANSP);
    ring.setArcLayout(30.0f, 132.0f);
    ring.setSpread(RING_SPREAD);
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
