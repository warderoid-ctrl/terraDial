#include "ui_lights.h"
#include "../net/terrapixel_client.h"
#include "../led/panel_ring.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include "ui_widgets.h"
#include <stdio.h>

namespace
{
    lv_obj_t *lightsPanel = nullptr; // the scrollable page, for knob scrolling
    lv_obj_t *connDot = nullptr;
    lv_obj_t *railModeLabel = nullptr;
    lv_obj_t *panelSlider = nullptr;
    lv_obj_t *filmSwitch = nullptr;
    lv_obj_t *railBrightSlider = nullptr;
    lv_obj_t *radiusSlider = nullptr;
    lv_obj_t *partyBtn = nullptr;
    lv_obj_t *partyBtnLbl = nullptr;

    bool suppressEvents = false; // true while we're programmatically syncing widgets

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
        // Non-blocking: recorded now, sent by the network task.
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
        // Fire-and-forget; uiLightsUpdate() picks the real state back up
        // once terraPixel has actually acknowledged the toggle.
        terraPixel.toggleParty();
    }
}

lv_obj_t *uiLightsCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    // Same full-face page as a Settings category (ui_widgets.h) rather than
    // the older inset card -- this screen has five controls and was the last
    // one still squeezing them into ~142px of width.
    lv_obj_t *panel = uiMakePanel(scr, "LIGHTS");
    lightsPanel = panel;

    // -- rail connection state --
    lv_obj_t *railHeader = lv_obj_create(panel);
    lv_obj_set_size(railHeader, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(railHeader, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(railHeader, 0, 0);
    lv_obj_set_style_pad_all(railHeader, 0, 0);
    lv_obj_set_flex_flow(railHeader, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(railHeader, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(railHeader, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(railHeader, LV_SCROLLBAR_MODE_OFF);

    connDot = lv_obj_create(railHeader);
    lv_obj_set_size(connDot, 10, 10);
    lv_obj_set_style_radius(connDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(connDot, Palette::textFaint(), 0);
    lv_obj_set_style_border_width(connDot, 0, 0);
    lv_obj_clear_flag(connDot, LV_OBJ_FLAG_CLICKABLE);

    railModeLabel = lv_label_create(railHeader);
    lv_label_set_text(railModeLabel, "Rail: --");
    lv_obj_set_style_text_font(railModeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(railModeLabel, Palette::textMuted(), 0);
    lv_obj_set_style_pad_left(railModeLabel, 6, 0);

    // -- panel ring (local, always available) --
    lv_obj_t *panelRow = uiMakeRow(panel, "Panel ring brightness");
    panelSlider = uiMakeSlider(panelRow, 0, 100, panelRing.brightness());
    lv_obj_add_event_cb(panelSlider, panelSliderCb, LV_EVENT_VALUE_CHANGED, NULL);

    // -- terraPixel rail --
    lv_obj_t *filmRow = uiMakeRow(panel, "Film mode");
    filmSwitch = uiMakeSwitch(filmRow, false);
    lv_obj_add_event_cb(filmSwitch, filmSwitchCb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *railBrightRow = uiMakeRow(panel, "Rail brightness");
    railBrightSlider = uiMakeSlider(railBrightRow, 10, 255, 200);
    lv_obj_add_event_cb(railBrightSlider, railBrightSliderCb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *radiusRow = uiMakeRow(panel, "Comet width");
    radiusSlider = uiMakeSlider(radiusRow, 10, 80, 35); // value/10 = LEDs (1.0-8.0)
    lv_obj_add_event_cb(radiusSlider, radiusSliderCb, LV_EVENT_VALUE_CHANGED, NULL);

    partyBtn = uiMakeButton(panel, "Party: OFF", &partyBtnLbl);
    lv_obj_add_event_cb(partyBtn, partyBtnCb, LV_EVENT_CLICKED, NULL);

    addBackButton(scr);
    return scr;
}

void uiLightsHandleRotate(int32_t delta)
{
    if (!lightsPanel || delta == 0) return;
    // Same step as a Settings category panel so scrolling feels identical
    // wherever the knob is scrolling a page rather than stepping a ring.
    lv_obj_scroll_by(lightsPanel, 0, -delta * 24, LV_ANIM_ON);
}

void uiLightsOnShow()
{
    // Asks the network task for fresh state rather than fetching it here --
    // this used to be a blocking HTTP call, so opening Lights with the
    // controller unreachable stalled the screen transition for ~1s.
    terraPixel.requestRefresh();
    const TerraPixelStatus &st = terraPixel.status();

    suppressEvents = true;

    lv_slider_set_value(panelSlider, panelRing.brightness(), LV_ANIM_OFF);

    if (st.reachable)
    {
        lv_obj_set_style_bg_color(connDot, Palette::accentSecondary(), 0);
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
        lv_obj_set_style_bg_color(connDot, Palette::textFaint(), 0);
        lv_label_set_text(railModeLabel, "Rail: unreachable");
    }

    suppressEvents = false;
}

void uiLightsUpdate()
{
    // A plain read of the cached status the network task maintains -- no
    // network work happens on this thread. Deliberately doesn't touch the
    // sliders or switch, so a poll can't yank a control out from under a
    // finger mid-drag.
    if (!connDot) return; // screen not created yet

    const TerraPixelStatus &st = terraPixel.status();
    lv_obj_set_style_bg_color(connDot, st.reachable ? Palette::accentSecondary() : Palette::textFaint(), 0);
    char buf[24];
    snprintf(buf, sizeof(buf), "Rail: %s", st.reachable ? st.mode : "unreachable");
    lv_label_set_text(railModeLabel, buf);
    lv_label_set_text(partyBtnLbl, st.party ? "Party: ON" : "Party: OFF");
}
