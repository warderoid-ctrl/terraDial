#include "ui_settings.h"
#include "../config/settings.h"
#include "../net/wifi_manager.h"
#include "carousel.h"
#include "palette.h"
#include "ui_widgets.h"
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

// Settings: a 4-card carousel (Wi-Fi, Machine, Display, About), replacing
// the old single scrolling list. Wi-Fi is new -- on-device SSID/password
// entry reusing the exact edit-overlay/keyboard pattern the hostname
// fields already used. Machine/Display carry over today's real settings
// unchanged; About shows real, always-available diagnostics (hostname/IP/
// uptime) rather than a fabricated version string, since none exists.
namespace
{
    lv_obj_t *screenRoot = nullptr;
    Carousel carousel;

    // -- shared text-entry overlay (keyboard + textarea), opened on
    // lv_layer_top() so it floats above whichever card is current --
    lv_obj_t *editOverlay = nullptr;
    lv_obj_t *editTextarea = nullptr;
    char *editTarget = nullptr;
    size_t editTargetSize = 0;
    void (*editOnSaved)() = nullptr;
    // Fires once the editor closes, on EITHER exit path (saved or
    // canceled) -- unlike onSaved. Used by the Wi-Fi password step to
    // always attempt a reconnect on the way out, so canceling never just
    // leaves the radio silently disconnected (see runScan()'s comment).
    void (*editOnClosed)() = nullptr;

    void closeEditor()
    {
        if (editOverlay)
        {
            lv_obj_del(editOverlay);
            editOverlay = nullptr;
            editTextarea = nullptr;
        }
        if (editOnClosed)
        {
            void (*cb)() = editOnClosed;
            editOnClosed = nullptr;
            cb();
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

    void cancelBtnCb(lv_event_t *e) { (void)e; closeEditor(); }

    void openEditor(char *target, size_t targetSize, void (*onSaved)(), bool isPassword, void (*onClosed)() = nullptr)
    {
        editTarget = target;
        editTargetSize = targetSize;
        editOnSaved = onSaved;
        editOnClosed = onClosed;

        editOverlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(editOverlay, 240, 240);
        lv_obj_set_style_bg_color(editOverlay, Palette::bgApp(), 0);
        lv_obj_set_style_bg_opa(editOverlay, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(editOverlay, 0, 0);
        lv_obj_set_style_pad_all(editOverlay, 4, 0);

        // Explicit, unmissable close affordance -- LVGL's default text
        // keyboard has no visible "Cancel"/X key of its own (the only way
        // to fire LV_EVENT_CANCEL from the stock map is tapping the small
        // keyboard-glyph key, which doesn't read as "cancel" to a user and
        // left this screen with no discoverable way out).
        //
        // TOP_MID, not TOP_RIGHT: this is a ROUND 240x240 panel -- a point
        // needs to stay within ~120px of screen center (120,120) to be on
        // the physical glass at all. TOP_RIGHT's corner is ~148px out,
        // entirely outside the visible circle (confirmed the actual bug
        // behind "still can't get out" -- the button existed but nothing
        // could ever tap it). Centered near the top stays inside the
        // circle, same reasoning as ui_screen_shell.cpp's header position.
        lv_obj_t *cancelBtn = lv_btn_create(editOverlay);
        lv_obj_set_size(cancelBtn, 28, 28);
        lv_obj_set_style_radius(cancelBtn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cancelBtn, Palette::bgTerminal(), 0);
        lv_obj_align(cancelBtn, LV_ALIGN_TOP_MID, 0, 14);
        lv_obj_add_event_cb(cancelBtn, cancelBtnCb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *cancelLbl = lv_label_create(cancelBtn);
        lv_label_set_text(cancelLbl, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_font(cancelLbl, &lv_font_montserrat_12, 0);
        lv_obj_center(cancelLbl);

        editTextarea = lv_textarea_create(editOverlay);
        lv_obj_set_size(editTextarea, 220, 40);
        lv_obj_align(editTextarea, LV_ALIGN_TOP_MID, 0, 50);
        lv_textarea_set_one_line(editTextarea, true);
        lv_textarea_set_password_mode(editTextarea, isPassword);
        lv_textarea_set_text(editTextarea, target);

        lv_obj_t *kb = lv_keyboard_create(editOverlay);
        lv_obj_set_size(kb, 240, 150);
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(kb, editTextarea);
        lv_obj_add_event_cb(kb, keyboardEventCb, LV_EVENT_ALL, NULL);
    }

    lv_obj_t *makeCardShell(const char *title)
    {
        lv_obj_t *card = lv_obj_create(screenRoot);
        lv_obj_set_style_bg_color(card, Palette::bgTerminal(), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 20, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 12, 0);
        lv_obj_set_style_pad_row(card, 8, 0);
        lv_obj_set_scroll_dir(card, LV_DIR_VER);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *titleLbl = lv_label_create(card);
        lv_label_set_text(titleLbl, title);
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(titleLbl, Palette::accentSecondary(), 0);

        return card;
    }

    // ---- Wi-Fi card ----
    lv_obj_t *ssidLbl = nullptr;
    lv_obj_t *passLbl = nullptr;
    lv_obj_t *wifiStatusLbl = nullptr;

    void refreshSsidLabel()
    {
        char buf[48];
        snprintf(buf, sizeof(buf), "SSID: %s", Config::get().wifiSsid);
        lv_label_set_text(ssidLbl, buf);
    }

    // ---- Network picker: scan and pick instead of typing an SSID ----
    // secrets.h's WIFI_SSID/WIFI_PASS stay the compile-time defaults
    // (seeded into NVS on first boot, see config/settings.cpp) -- this
    // only changes how a *different* network gets entered on-device.
    lv_obj_t *scanOverlay = nullptr;
    lv_obj_t *scanList = nullptr;
    lv_obj_t *scanStatusLbl = nullptr;

    void closeScanOverlay()
    {
        if (scanOverlay)
        {
            lv_obj_del(scanOverlay);
            scanOverlay = nullptr;
            scanList = nullptr;
            scanStatusLbl = nullptr;
        }
    }

    // Reconnects using whatever's currently in Config -- called on every
    // way out of the scan+password flow (Cancel at either step, or a
    // completed password entry) so the radio never ends up silently
    // parked disconnected just because scanning required disconnecting
    // first (see runScan()). If nothing was actually picked, this just
    // restores the network that was already configured.
    void reconnectAfterWifiEdit()
    {
        WifiManager::reconnect(Config::get().wifiSsid, Config::get().wifiPass);
    }

    void networkPickedCb(lv_event_t *e)
    {
        lv_obj_t *btn = lv_event_get_target(e);
        const char *ssid = lv_list_get_btn_text(scanList, btn);
        strncpy(Config::get().wifiSsid, ssid, sizeof(Config::get().wifiSsid) - 1);
        Config::get().wifiSsid[sizeof(Config::get().wifiSsid) - 1] = '\0';
        Config::save();
        refreshSsidLabel();
        closeScanOverlay();
        // Naturally flows into typing the password for the network just
        // picked -- password-editing itself is unchanged.
        openEditor(Config::get().wifiPass, sizeof(Config::get().wifiPass), nullptr, true, reconnectAfterWifiEdit);
    }

    void runScan()
    {
        lv_obj_clean(scanList);
        lv_label_set_text(scanStatusLbl, "Scanning...");
        // Scanning fails immediately (not after a timeout) if the radio is
        // mid-connect/retry -- confirmed against the Arduino core's
        // WiFiScan.cpp: esp_wifi_scan_start() needs an idle STA. Stop any
        // in-progress connection attempt first so the scan can actually
        // run; every exit from this flow reconnects afterward (see
        // reconnectAfterWifiEdit/scanCancelCb), so this never leaves the
        // device stranded off-network.
        WiFi.disconnect(false, false);
        delay(100);
        int n = WiFi.scanNetworks();
        Serial.printf("[settings] WiFi.scanNetworks() -> %d\n", n);
        if (n == WIFI_SCAN_FAILED)
        {
            lv_label_set_text(scanStatusLbl, "Scan failed -- try Rescan");
            return;
        }
        if (n == 0)
        {
            lv_label_set_text(scanStatusLbl, "No networks found nearby");
            return;
        }
        lv_label_set_text(scanStatusLbl, "");
        for (int i = 0; i < n; i++)
        {
            lv_obj_t *btn = lv_list_add_btn(scanList, LV_SYMBOL_WIFI, WiFi.SSID(i).c_str());
            lv_obj_set_style_bg_color(btn, Palette::bgPanel(), 0);
            lv_obj_add_event_cb(btn, networkPickedCb, LV_EVENT_CLICKED, NULL);
        }
        WiFi.scanDelete();
    }

    void rescanCb(lv_event_t *e) { (void)e; runScan(); }

    void scanCancelCb(lv_event_t *e)
    {
        (void)e;
        closeScanOverlay();
        reconnectAfterWifiEdit();
    }

    void openScanOverlay()
    {
        scanOverlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(scanOverlay, 240, 240);
        lv_obj_set_style_bg_color(scanOverlay, Palette::bgApp(), 0);
        lv_obj_set_style_bg_opa(scanOverlay, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(scanOverlay, 0, 0);
        lv_obj_clear_flag(scanOverlay, LV_OBJ_FLAG_SCROLLABLE);

        // Deliberate, individually-placed positions rather than a flex
        // layout: this is a round screen, and flex's top-packed stacking
        // pushed content into the corners/edges the physical glass doesn't
        // cover (same root cause as the cancel-button fix in openEditor()
        // above -- flex layout also silently overrides any manual
        // lv_obj_align() on its own children, so the two don't mix here).
        lv_obj_t *cancelBtn = lv_btn_create(scanOverlay);
        lv_obj_set_size(cancelBtn, 28, 28);
        lv_obj_set_style_radius(cancelBtn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(cancelBtn, Palette::bgTerminal(), 0);
        lv_obj_align(cancelBtn, LV_ALIGN_TOP_MID, 0, 14);
        lv_obj_add_event_cb(cancelBtn, scanCancelCb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *cancelLbl = lv_label_create(cancelBtn);
        lv_label_set_text(cancelLbl, LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_font(cancelLbl, &lv_font_montserrat_12, 0);
        lv_obj_center(cancelLbl);

        lv_obj_t *titleLbl = lv_label_create(scanOverlay);
        lv_label_set_text(titleLbl, "Select a network");
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(titleLbl, lv_color_white(), 0);
        lv_obj_align(titleLbl, LV_ALIGN_TOP_MID, 0, 46);

        scanStatusLbl = lv_label_create(scanOverlay);
        lv_obj_set_style_text_font(scanStatusLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(scanStatusLbl, Palette::textMuted(), 0);
        lv_obj_align(scanStatusLbl, LV_ALIGN_TOP_MID, 0, 68);

        scanList = lv_list_create(scanOverlay);
        lv_obj_set_size(scanList, 190, 108);
        lv_obj_align(scanList, LV_ALIGN_CENTER, 0, 8);
        lv_obj_set_style_bg_opa(scanList, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(scanList, 0, 0);

        lv_obj_t *rescanBtn = lv_btn_create(scanOverlay);
        lv_obj_set_size(rescanBtn, 100, 30);
        lv_obj_set_style_bg_color(rescanBtn, Palette::bgTerminal(), 0);
        lv_obj_set_style_radius(rescanBtn, 15, 0);
        lv_obj_align(rescanBtn, LV_ALIGN_BOTTOM_MID, 0, -16);
        lv_obj_add_event_cb(rescanBtn, rescanCb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *rescanLbl = lv_label_create(rescanBtn);
        lv_label_set_text(rescanLbl, "Rescan");
        lv_obj_set_style_text_font(rescanLbl, &lv_font_montserrat_12, 0);
        lv_obj_center(rescanLbl);

        runScan();
    }

    void ssidCb(lv_event_t *e)
    {
        (void)e;
        openScanOverlay();
    }

    void passCb(lv_event_t *e)
    {
        (void)e;
        openEditor(Config::get().wifiPass, sizeof(Config::get().wifiPass), nullptr, true, reconnectAfterWifiEdit);
    }

    void connectCb(lv_event_t *e)
    {
        (void)e;
        lv_label_set_text(wifiStatusLbl, "Connecting...");
        WifiManager::reconnect(Config::get().wifiSsid, Config::get().wifiPass);
    }

    lv_obj_t *makeWifiCard()
    {
        lv_obj_t *card = makeCardShell("WI-FI");

        ssidLbl = lv_label_create(card);
        lv_obj_set_style_text_font(ssidLbl, &lv_font_montserrat_12, 0);
        lv_obj_add_flag(ssidLbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(ssidLbl, ssidCb, LV_EVENT_CLICKED, NULL);
        refreshSsidLabel();

        passLbl = lv_label_create(card);
        lv_label_set_text(passLbl, "Password: (tap to edit)");
        lv_obj_set_style_text_font(passLbl, &lv_font_montserrat_12, 0);
        lv_obj_add_flag(passLbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(passLbl, passCb, LV_EVENT_CLICKED, NULL);

        wifiStatusLbl = lv_label_create(card);
        lv_obj_set_style_text_font(wifiStatusLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(wifiStatusLbl, Palette::textMuted(), 0);
        lv_label_set_text(wifiStatusLbl, "--");

        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_size(btn, 110, 34);
        lv_obj_set_style_bg_color(btn, Palette::accent(), 0);
        lv_obj_set_style_radius(btn, 17, 0);
        lv_obj_add_event_cb(btn, connectCb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *btnLbl = lv_label_create(btn);
        lv_label_set_text(btnLbl, "Connect");
        lv_obj_set_style_text_color(btnLbl, Palette::accentFg(), 0);
        lv_obj_center(btnLbl);

        return card;
    }

    // ---- Machine card (FluidNC/terraPixel hosts, pen jog) ----
    lv_obj_t *fncHostLbl = nullptr;
    lv_obj_t *tpHostLbl = nullptr;
    lv_obj_t *penMmLbl = nullptr;
    lv_obj_t *penFeedLbl = nullptr;

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
        openEditor(Config::get().fluidNcHost, sizeof(Config::get().fluidNcHost), refreshHostLabels, false);
    }

    void tpHostCb(lv_event_t *e)
    {
        (void)e;
        openEditor(Config::get().terraPixelHost, sizeof(Config::get().terraPixelHost), refreshHostLabels, false);
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

    lv_obj_t *makeMachineCard()
    {
        lv_obj_t *card = makeCardShell("MACHINE");

        fncHostLbl = lv_label_create(card);
        lv_obj_set_style_text_font(fncHostLbl, &lv_font_montserrat_12, 0);
        lv_obj_add_flag(fncHostLbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(fncHostLbl, fncHostCb, LV_EVENT_CLICKED, NULL);

        tpHostLbl = lv_label_create(card);
        lv_obj_set_style_text_font(tpHostLbl, &lv_font_montserrat_12, 0);
        lv_obj_add_flag(tpHostLbl, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tpHostLbl, tpHostCb, LV_EVENT_CLICKED, NULL);
        refreshHostLabels();

        lv_obj_t *penMmRow = uiMakeRow(card);
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

        lv_obj_t *penFeedRow = uiMakeRow(card);
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

        return card;
    }

    // ---- Display card (backlight timeout) ----
    lv_obj_t *timeoutLbl = nullptr;

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

    lv_obj_t *makeDisplayCard()
    {
        lv_obj_t *card = makeCardShell("DISPLAY");

        lv_obj_t *timeoutRow = uiMakeRow(card);
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

        return card;
    }

    // ---- About card (real diagnostics, nothing fabricated) ----
    lv_obj_t *aboutIpLbl = nullptr;
    lv_obj_t *aboutUptimeLbl = nullptr;

    lv_obj_t *makeAboutCard()
    {
        lv_obj_t *card = makeCardShell("ABOUT");

        lv_obj_t *hostLbl = lv_label_create(card);
        lv_label_set_text(hostLbl, "terratouch.local");
        lv_obj_set_style_text_font(hostLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(hostLbl, lv_color_white(), 0);

        aboutIpLbl = lv_label_create(card);
        lv_obj_set_style_text_font(aboutIpLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(aboutIpLbl, Palette::textMuted(), 0);

        aboutUptimeLbl = lv_label_create(card);
        lv_obj_set_style_text_font(aboutUptimeLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(aboutUptimeLbl, Palette::textMuted(), 0);

        return card;
    }
}

lv_obj_t *uiSettingsCreate()
{
    screenRoot = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screenRoot, Palette::bgApp(), 0);

    // Bigger center card than Home/Jobs (186 matches the round-safe
    // content area ui_screen_shell.cpp already uses) -- these cards hold
    // several rows of controls, not just an icon+label.
    carousel.create(screenRoot, 186, 90);
    carousel.addCard(makeWifiCard());
    carousel.addCard(makeMachineCard());
    carousel.addCard(makeDisplayCard());
    carousel.addCard(makeAboutCard());

    return screenRoot;
}

void uiSettingsHandleRotate(int32_t delta)
{
    if (delta == 0) return;
    for (int32_t i = 0; i < delta; i++) carousel.selectNext();
    for (int32_t i = 0; i < -delta; i++) carousel.selectPrev();
}

void uiSettingsUpdate()
{
    if (!wifiStatusLbl) return;

    wl_status_t status = WiFi.status();
    lv_label_set_text(wifiStatusLbl, status == WL_CONNECTED ? "Connected" : "Not connected");

    char buf[32];
    if (status == WL_CONNECTED) snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
    else snprintf(buf, sizeof(buf), "IP: --");
    lv_label_set_text(aboutIpLbl, buf);

    uint32_t upSec = millis() / 1000;
    snprintf(buf, sizeof(buf), "Uptime: %luh %lum", (unsigned long)(upSec / 3600), (unsigned long)((upSec / 60) % 60));
    lv_label_set_text(aboutUptimeLbl, buf);
}
