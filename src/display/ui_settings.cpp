#include "ui_settings.h"
#include "../config/settings.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <stdio.h>
#include <string.h>

namespace
{
    lv_obj_t *fncHostLbl = nullptr;
    lv_obj_t *tpHostLbl = nullptr;
    lv_obj_t *timeoutLbl = nullptr;
    lv_obj_t *penMmLbl = nullptr;
    lv_obj_t *penFeedLbl = nullptr;

    // Text-entry overlay (keyboard + textarea), shared by both hostname
    // fields -- opened on lv_layer_top() so it floats above whichever
    // screen is currently loaded.
    lv_obj_t *editOverlay = nullptr;
    lv_obj_t *editTextarea = nullptr;
    char *editTarget = nullptr;
    size_t editTargetSize = 0;
    void (*editOnSaved)() = nullptr;

    void closeEditor()
    {
        if (editOverlay)
        {
            lv_obj_del(editOverlay);
            editOverlay = nullptr;
            editTextarea = nullptr;
        }
    }

    void keyboardEventCb(lv_event_t *e)
    {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_READY)
        {
            if (editTarget && editTargetSize > 0)
            {
                const char *txt = lv_textarea_get_text(editTextarea);
                strncpy(editTarget, txt, editTargetSize - 1);
                editTarget[editTargetSize - 1] = '\0';
                Config::save();
                if (editOnSaved) editOnSaved();
            }
            closeEditor();
        }
        else if (code == LV_EVENT_CANCEL)
        {
            closeEditor();
        }
    }

    void openEditor(char *target, size_t targetSize, void (*onSaved)())
    {
        editTarget = target;
        editTargetSize = targetSize;
        editOnSaved = onSaved;

        editOverlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(editOverlay, 240, 240);
        lv_obj_set_style_bg_color(editOverlay, Palette::bgApp(), 0);
        lv_obj_set_style_bg_opa(editOverlay, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(editOverlay, 0, 0);
        lv_obj_set_style_pad_all(editOverlay, 4, 0);

        editTextarea = lv_textarea_create(editOverlay);
        lv_obj_set_size(editTextarea, 220, 40);
        lv_obj_align(editTextarea, LV_ALIGN_TOP_MID, 0, 18);
        lv_textarea_set_one_line(editTextarea, true);
        lv_textarea_set_text(editTextarea, target);

        lv_obj_t *kb = lv_keyboard_create(editOverlay);
        lv_obj_set_size(kb, 240, 150);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(kb, editTextarea);
        lv_obj_add_event_cb(kb, keyboardEventCb, LV_EVENT_ALL, NULL);
    }

    void refreshHostLabels()
    {
        char buf[40];
        snprintf(buf, sizeof(buf), "FluidNC: %s", Config::get().fluidNcHost);
        lv_label_set_text(fncHostLbl, buf);
        snprintf(buf, sizeof(buf), "terraPixel: %s", Config::get().terraPixelHost);
        lv_label_set_text(tpHostLbl, buf);
    }

    void fncHostCb(lv_event_t *e)
    {
        (void)e;
        openEditor(Config::get().fluidNcHost, sizeof(Config::get().fluidNcHost), refreshHostLabels);
    }

    void tpHostCb(lv_event_t *e)
    {
        (void)e;
        openEditor(Config::get().terraPixelHost, sizeof(Config::get().terraPixelHost), refreshHostLabels);
    }

    void timeoutSliderCb(lv_event_t *e)
    {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        int v = lv_slider_get_value(slider);
        char buf[24];
        if (v == 0) snprintf(buf, sizeof(buf), "Backlight: always on");
        else snprintf(buf, sizeof(buf), "Backlight: %ds", v);
        lv_label_set_text(timeoutLbl, buf);

        if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        {
            Config::get().backlightTimeoutSec = (uint16_t)v;
            Config::save();
        }
    }

    void penMmSliderCb(lv_event_t *e)
    {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        float mm = lv_slider_get_value(slider) / 10.0f;
        char buf[24];
        snprintf(buf, sizeof(buf), "Pen jog: %.1fmm", mm);
        lv_label_set_text(penMmLbl, buf);

        if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        {
            Config::get().penJogMm = mm;
            Config::save();
        }
    }

    void penFeedSliderCb(lv_event_t *e)
    {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        int feed = lv_slider_get_value(slider);
        char buf[28];
        snprintf(buf, sizeof(buf), "Pen feed: %dmm/min", feed);
        lv_label_set_text(penFeedLbl, buf);

        if (lv_event_get_code(e) == LV_EVENT_RELEASED)
        {
            Config::get().penJogFeed = (float)feed;
            Config::save();
        }
    }

    lv_obj_t *makeRow(lv_obj_t *parent)
    {
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        return row;
    }
}

lv_obj_t *uiSettingsCreate()
{
    ScreenShell shell = createScreenShell("SETTINGS", LV_SYMBOL_SETTINGS);
    lv_obj_t *panel = shell.content;
    // Same reasoning as Lights: more rows than fit at once, so pack from
    // the top instead of the shell's default vertical centering.
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // -- hostnames (tap to edit) --
    fncHostLbl = lv_label_create(panel);
    lv_obj_set_style_text_font(fncHostLbl, &lv_font_montserrat_12, 0);
    lv_obj_add_flag(fncHostLbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fncHostLbl, fncHostCb, LV_EVENT_CLICKED, NULL);

    tpHostLbl = lv_label_create(panel);
    lv_obj_set_style_text_font(tpHostLbl, &lv_font_montserrat_12, 0);
    lv_obj_add_flag(tpHostLbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tpHostLbl, tpHostCb, LV_EVENT_CLICKED, NULL);

    refreshHostLabels();

    // -- backlight timeout --
    lv_obj_t *timeoutRow = makeRow(panel);
    timeoutLbl = lv_label_create(timeoutRow);
    lv_obj_set_style_text_font(timeoutLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(timeoutLbl, Palette::textMuted(), 0);
    lv_obj_t *timeoutSlider = lv_slider_create(timeoutRow);
    lv_obj_set_width(timeoutSlider, lv_pct(100));
    lv_slider_set_range(timeoutSlider, 0, 300);
    lv_slider_set_value(timeoutSlider, Config::get().backlightTimeoutSec, LV_ANIM_OFF);
    lv_obj_add_event_cb(timeoutSlider, timeoutSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(timeoutSlider, timeoutSliderCb, LV_EVENT_RELEASED, NULL);
    {
        char buf[24];
        if (Config::get().backlightTimeoutSec == 0) snprintf(buf, sizeof(buf), "Backlight: always on");
        else snprintf(buf, sizeof(buf), "Backlight: %ds", Config::get().backlightTimeoutSec);
        lv_label_set_text(timeoutLbl, buf);
    }

    // -- pen jog distance --
    lv_obj_t *penMmRow = makeRow(panel);
    penMmLbl = lv_label_create(penMmRow);
    lv_obj_set_style_text_font(penMmLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(penMmLbl, Palette::textMuted(), 0);
    lv_obj_t *penMmSlider = lv_slider_create(penMmRow);
    lv_obj_set_width(penMmSlider, lv_pct(100));
    lv_slider_set_range(penMmSlider, 5, 200); // 0.5mm - 20.0mm
    lv_slider_set_value(penMmSlider, (int)(Config::get().penJogMm * 10.0f), LV_ANIM_OFF);
    lv_obj_add_event_cb(penMmSlider, penMmSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(penMmSlider, penMmSliderCb, LV_EVENT_RELEASED, NULL);
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "Pen jog: %.1fmm", Config::get().penJogMm);
        lv_label_set_text(penMmLbl, buf);
    }

    // -- pen jog feed --
    lv_obj_t *penFeedRow = makeRow(panel);
    penFeedLbl = lv_label_create(penFeedRow);
    lv_obj_set_style_text_font(penFeedLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(penFeedLbl, Palette::textMuted(), 0);
    lv_obj_t *penFeedSlider = lv_slider_create(penFeedRow);
    lv_obj_set_width(penFeedSlider, lv_pct(100));
    lv_slider_set_range(penFeedSlider, 100, 3000);
    lv_slider_set_value(penFeedSlider, (int)Config::get().penJogFeed, LV_ANIM_OFF);
    lv_obj_add_event_cb(penFeedSlider, penFeedSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(penFeedSlider, penFeedSliderCb, LV_EVENT_RELEASED, NULL);
    {
        char buf[28];
        snprintf(buf, sizeof(buf), "Pen feed: %.0fmm/min", Config::get().penJogFeed);
        lv_label_set_text(penFeedLbl, buf);
    }

    return shell.screen;
}
