#include "ui_files.h"
#include "../net/fluidnc_client.h"
#include "carousel.h"
#include "palette.h"
#include <stdio.h>
#include <string.h>

// "Jobs" in the mockup -- same SD-card file browsing this screen always
// did, now paged as a carousel instead of a scroll list. The mockup's
// per-file meta line ("1.2k pts · ~34 min") has no real source: FluidNC's
// $SD/ListJSON only reports name+size (see fluidnc_client.cpp), so the
// card shows real size instead of an invented point count/ETA.
namespace
{
    Carousel carousel;
    lv_obj_t *screenRoot = nullptr;
    char selectedFile[48] = {0};

    void formatSize(char *buf, size_t bufSize, int32_t size)
    {
        if (size >= 1024 * 1024) snprintf(buf, bufSize, "%.1f MB", size / 1048576.0f);
        else if (size >= 1024) snprintf(buf, bufSize, "%.1f KB", size / 1024.0f);
        else snprintf(buf, bufSize, "%ld B", (long)size);
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

    lv_obj_t *makeCard(const FluidNCFileEntry &f)
    {
        lv_obj_t *card = lv_obj_create(screenRoot);
        lv_obj_set_style_bg_color(card, Palette::bgTerminal(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 20, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(card, 4, 0);

        lv_obj_t *iconLbl = lv_label_create(card);
        lv_label_set_text(iconLbl, LV_SYMBOL_FILE);
        lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(iconLbl, Palette::accent(), 0);

        // Long filenames are the normal case, not an edge case (per the
        // mockup) -- truncate with an ellipsis, never wrap.
        lv_obj_t *nameLbl = lv_label_create(card);
        lv_obj_set_width(nameLbl, 118);
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(nameLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(nameLbl, f.name);
        lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(nameLbl, lv_color_white(), 0);

        char sizeBuf[16];
        formatSize(sizeBuf, sizeof(sizeBuf), f.size);
        lv_obj_t *metaLbl = lv_label_create(card);
        lv_label_set_text(metaLbl, sizeBuf);
        lv_obj_set_style_text_font(metaLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(metaLbl, Palette::textMuted(), 0);

        return card;
    }

    lv_obj_t *makeEmptyCard()
    {
        lv_obj_t *card = lv_obj_create(screenRoot);
        lv_obj_set_style_bg_color(card, Palette::bgTerminal(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 20, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, "No G-code\nfiles found");
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(lbl, Palette::textMuted(), 0);
        return card;
    }

    void rebuildList()
    {
        carousel.clear();
        int count = fluidNC.fileListCount();
        if (count == 0)
        {
            carousel.addCard(makeEmptyCard());
            return;
        }
        for (int i = 0; i < count && i < FluidNCClient::MAX_FILES; i++)
            carousel.addCard(makeCard(fluidNC.fileListEntry(i)));
    }

    void onCardOpen(int index)
    {
        int count = fluidNC.fileListCount();
        if (index < 0 || index >= count) return;
        fluidNC.runFile(fluidNC.fileListEntry(index).name);
    }
}

lv_obj_t *uiFilesCreate()
{
    screenRoot = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screenRoot, Palette::bgApp(), 0);

    carousel.create(screenRoot);
    carousel.setOnOpen(onCardOpen);
    carousel.addCard(makeEmptyCard()); // placeholder until the first listing arrives

    return screenRoot;
}

void uiFilesSetFocused(bool focused)
{
    if (focused) fluidNC.requestFileList();
}

void uiFilesHandleRotate(int32_t delta)
{
    if (delta == 0) return;
    for (int32_t i = 0; i < delta; i++) carousel.selectNext();
    for (int32_t i = 0; i < -delta; i++) carousel.selectPrev();
}

void uiFilesHandleSelect()
{
    carousel.openSelected();
}

void uiFilesHandleDoubleClick()
{
    int count = fluidNC.fileListCount();
    int idx = carousel.selectedIndex();
    if (idx < 0 || idx >= count) return;
    openConfirmFor(fluidNC.fileListEntry(idx).name);
}

void uiFilesUpdate()
{
    if (!fluidNC.fileListReady()) return;
    fluidNC.clearFileListReady();
    rebuildList();
}
