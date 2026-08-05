#include "ui_lights.h"
#include "../net/terrapixel_client.h"
#include "../led/panel_ring.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include <stdio.h>

namespace
{
    lv_obj_t *connDot = nullptr;
    lv_obj_t *railModeLabel = nullptr;
    lv_obj_t *panelSlider = nullptr;
    lv_obj_t *filmSwitch = nullptr;
    lv_obj_t *railBrightSlider = nullptr;
    lv_obj_t *radiusSlider = nullptr;
    lv_obj_t *partyBtn = nullptr;
    lv_obj_t *partyBtnLbl = nullptr;

    bool suppressEvents = false; // true while we're programmatically syncing widgets

    lv_obj_t *makeRow(lv_obj_t *parent, const char *labelText)
    {
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, labelText);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, Palette::textMuted(), 0);

        return row;
    }

    void panelSliderCb(lv_event_t *e)
    {
        (void)e;
        if (suppressEvents) return;
        panelRing.setBrightness((uint8_t)lv_slider_get_value(panelSlider));
    }

    void filmSwitchCb(lv_event_t *e)
    {
        (void)e;
        if (suppressEvents) return;
        terraPixel.setFilmMode(lv_obj_has_state(filmSwitch, LV_STATE_CHECKED));
    }

    void railBrightSliderCb(lv_event_t *e)
    {
        (void)e;
        if (suppressEvents) return;
        terraPixel.setBrightness((uint8_t)lv_slider_get_value(railBrightSlider));
    }

    void radiusSliderCb(lv_event_t *e)
    {
        (void)e;
        if (suppressEvents) return;
        float radius = lv_slider_get_value(radiusSlider) / 10.0f;
        terraPixel.setRadius(radius);
    }

    void partyBtnCb(lv_event_t *e)
    {
        (void)e;
        if (terraPixel.toggleParty())
        {
            lv_label_set_text(partyBtnLbl, terraPixel.status().party ? "Party: ON" : "Party: OFF");
        }
    }
}

lv_obj_t *uiLightsCreate()
{
    ScreenShell shell = createScreenShell("LIGHTS", LV_SYMBOL_TINT);
    lv_obj_t *panel = shell.content;
    // This screen has more rows than fit at once -- pack from the top
    // (scrollable) rather than the shell's default vertical centering,
    // which would start the view mid-list.
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // -- Panel ring --
    lv_obj_t *panelRow = makeRow(panel, "Panel ring brightness");
    panelSlider = lv_slider_create(panelRow);
    lv_obj_set_width(panelSlider, lv_pct(100));
    lv_slider_set_range(panelSlider, 0, 100);
    lv_obj_add_event_cb(panelSlider, panelSliderCb, LV_EVENT_VALUE_CHANGED, NULL);

    // -- terraPixel (rail) --
    lv_obj_t *railHeader = lv_obj_create(panel);
    lv_obj_set_size(railHeader, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(railHeader, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(railHeader, 0, 0);
    lv_obj_set_style_pad_all(railHeader, 2, 0);
    lv_obj_set_flex_flow(railHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(railHeader, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    connDot = lv_obj_create(railHeader);
    lv_obj_set_size(connDot, 10, 10);
    lv_obj_set_style_radius(connDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(connDot, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(connDot, 0, 0);
    lv_obj_clear_flag(connDot, LV_OBJ_FLAG_CLICKABLE);

    railModeLabel = lv_label_create(railHeader);
    lv_label_set_text(railModeLabel, "Rail: --");
    lv_obj_set_style_text_font(railModeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(railModeLabel, Palette::textMuted(), 0);
    lv_obj_set_style_pad_left(railModeLabel, 6, 0);

    lv_obj_t *filmRow = makeRow(panel, "Film mode");
    filmSwitch = lv_switch_create(filmRow);
    lv_obj_add_event_cb(filmSwitch, filmSwitchCb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *railBrightRow = makeRow(panel, "Rail brightness");
    railBrightSlider = lv_slider_create(railBrightRow);
    lv_obj_set_width(railBrightSlider, lv_pct(100));
    lv_slider_set_range(railBrightSlider, 10, 255);
    lv_obj_add_event_cb(railBrightSlider, railBrightSliderCb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *radiusRow = makeRow(panel, "Comet width");
    radiusSlider = lv_slider_create(radiusRow);
    lv_obj_set_width(radiusSlider, lv_pct(100));
    lv_slider_set_range(radiusSlider, 10, 80); // value/10 = LEDs (1.0-8.0)
    lv_obj_add_event_cb(radiusSlider, radiusSliderCb, LV_EVENT_VALUE_CHANGED, NULL);

    partyBtn = lv_btn_create(panel);
    lv_obj_set_width(partyBtn, lv_pct(100));
    lv_obj_set_style_bg_color(partyBtn, lv_color_hex(0x6a1b9a), 0);
    lv_obj_add_event_cb(partyBtn, partyBtnCb, LV_EVENT_CLICKED, NULL);
    partyBtnLbl = lv_label_create(partyBtn);
    lv_label_set_text(partyBtnLbl, "Party: OFF");
    lv_obj_center(partyBtnLbl);

    return shell.screen;
}

void uiLightsOnShow()
{
    terraPixel.refreshStatus();
    const TerraPixelStatus &st = terraPixel.status();

    suppressEvents = true;

    lv_slider_set_value(panelSlider, panelRing.brightness(), LV_ANIM_OFF);

    if (st.reachable)
    {
        lv_obj_set_style_bg_color(connDot, lv_color_hex(0x22cc44), 0);
        char buf[24];
        snprintf(buf, sizeof(buf), "Rail: %s", st.mode);
        lv_label_set_text(railModeLabel, buf);

        if (st.filmMode) lv_obj_add_state(filmSwitch, LV_STATE_CHECKED);
        else lv_obj_clear_state(filmSwitch, LV_STATE_CHECKED);

        lv_slider_set_value(railBrightSlider, st.brightness, LV_ANIM_OFF);
        lv_slider_set_value(radiusSlider, (int)(st.radius * 10.0f), LV_ANIM_OFF);
        lv_label_set_text(partyBtnLbl, st.party ? "Party: ON" : "Party: OFF");
    }
    else
    {
        lv_obj_set_style_bg_color(connDot, lv_color_hex(0x555555), 0);
        lv_label_set_text(railModeLabel, "Rail: unreachable");
    }

    suppressEvents = false;
}

void uiLightsUpdate()
{
    static uint32_t lastRefresh = 0;
    uint32_t now = millis();
    if (now - lastRefresh < 3000) return;
    lastRefresh = now;

    if (!terraPixel.refreshStatus()) return;
    if (!connDot) return; // screen not created yet

    const TerraPixelStatus &st = terraPixel.status();
    lv_obj_set_style_bg_color(connDot, st.reachable ? lv_color_hex(0x22cc44) : lv_color_hex(0x555555), 0);
    char buf[24];
    snprintf(buf, sizeof(buf), "Rail: %s", st.reachable ? st.mode : "unreachable");
    lv_label_set_text(railModeLabel, buf);
}
