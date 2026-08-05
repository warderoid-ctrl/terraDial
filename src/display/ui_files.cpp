#include "ui_files.h"
#include "../net/fluidnc_client.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <string.h>
#include <stdio.h>

namespace
{
    lv_obj_t *focusRing = nullptr;
    lv_obj_t *list = nullptr;
    lv_obj_t *emptyLabel = nullptr;

    lv_obj_t *itemBtns[FluidNCClient::MAX_FILES] = {nullptr};
    int shownCount = 0;
    int highlightIndex = -1;

    char selectedFile[48] = {0};

    void restyleHighlight()
    {
        for (int i = 0; i < shownCount; i++)
        {
            lv_obj_set_style_bg_color(itemBtns[i], i == highlightIndex ? Palette::selectedFill() : Palette::bgPanel(), 0);
        }
        if (highlightIndex >= 0 && highlightIndex < shownCount)
        {
            lv_obj_scroll_to_view(itemBtns[highlightIndex], LV_ANIM_ON);
        }
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

    void rebuildList()
    {
        lv_obj_clean(list);
        shownCount = 0;
        highlightIndex = -1;

        int count = fluidNC.fileListCount();
        if (count == 0)
        {
            emptyLabel = lv_label_create(list);
            lv_label_set_text(emptyLabel, "No G-code files found");
            lv_obj_set_style_text_color(emptyLabel, Palette::textMuted(), 0);
            return;
        }

        for (int i = 0; i < count && i < FluidNCClient::MAX_FILES; i++)
        {
            const FluidNCFileEntry &f = fluidNC.fileListEntry(i);
            // No icon: on a 240px round panel the list is only ~190px wide
            // (narrower still near the circular clip's top/bottom rows), so
            // the file-type icon isn't worth the horizontal space it costs
            // long filenames. Deeper round-aware layout (truncation with
            // ellipsis, marquee scroll) is deferred to the later GUI pass.
            itemBtns[i] = lv_list_add_btn(list, NULL, f.name);
            lv_obj_set_style_bg_color(itemBtns[i], Palette::bgPanel(), 0);
        }
        shownCount = count;
        highlightIndex = 0;
        restyleHighlight();
    }
}

lv_obj_t *uiFilesCreate()
{
    ScreenShell shell = createScreenShell("FILES", LV_SYMBOL_FILE);
    focusRing = shell.ring;

    list = lv_list_create(shell.content);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    emptyLabel = lv_label_create(list);
    lv_label_set_text(emptyLabel, "click to load...");
    lv_obj_set_style_text_color(emptyLabel, Palette::textMuted(), 0);

    return shell.screen;
}

void uiFilesSetFocused(bool focused)
{
    if (!focusRing) return;
    lv_obj_set_style_arc_color(focusRing, focused ? Palette::accent() : Palette::border(), LV_PART_INDICATOR);
    if (focused) fluidNC.requestFileList();
}

void uiFilesHandleRotate(int32_t delta)
{
    if (delta == 0 || shownCount == 0) return;
    highlightIndex += (delta > 0 ? 1 : -1);
    if (highlightIndex < 0) highlightIndex = 0;
    if (highlightIndex >= shownCount) highlightIndex = shownCount - 1;
    restyleHighlight();
}

void uiFilesHandleSelect()
{
    if (highlightIndex < 0 || highlightIndex >= shownCount) return;
    openConfirmFor(fluidNC.fileListEntry(highlightIndex).name);
}

void uiFilesUpdate()
{
    if (!fluidNC.fileListReady()) return;
    fluidNC.clearFileListReady();
    rebuildList();
}
