#include "ui_settings.h"
#include "../config/settings.h"
#include "../net/wifi_manager.h"
#include "radial_ring.h"
#include "ui_nav.h"
#include "palette.h"
#include "ui_widgets.h"
#include "lgfx_config.h" // backlightSet()
#include "radial_keyboard.h"
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

// Settings: a radial ring of four categories (Wi-Fi, Machine, Display,
// About), matching Home and Jobs. Selecting one swaps the ring out for that
// category's controls; back returns to the ring.
//
// It was a carousel of four permanently-visible cards -- the last screen
// still using a different browsing idiom, and a card big enough to hold
// several controls leaves nothing to peek at, so the carousel earned
// nothing over a ring. Splitting into "pick a category, then see its
// controls" also gives each control the full width of the panel.
//
// The four panels are built once and kept hidden rather than created on
// demand: the label pointers below (wifiStatusLbl, aboutIpLbl, ...) are
// refreshed every loop by uiSettingsUpdate(), and destroying the panels
// would leave those dangling.
namespace
{
    lv_obj_t *screenRoot = nullptr;
    RadialRing ring;
    lv_obj_t *hub = nullptr;
    lv_obj_t *hubNameLbl = nullptr;
    lv_obj_t *hubHintLbl = nullptr;

    const int CATEGORY_COUNT = 4;
    lv_obj_t *panels[CATEGORY_COUNT] = {nullptr};
    int openPanel = -1; // -1 = showing the ring

    const char *CATEGORY_NAMES[CATEGORY_COUNT] = {"Wi-Fi", "Machine", "Display", "About"};
    const char *CATEGORY_ICONS[CATEGORY_COUNT] = {
        LV_SYMBOL_WIFI, LV_SYMBOL_DRIVE, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_LIST};

    // -- shared text entry --
    // Delegates to the radial keyboard (display/radial_keyboard.h) rather
    // than owning an lv_keyboard overlay: on a 240px round panel a QWERTY
    // map's keys are narrower than a fingertip, so text entry is knob-first
    // here.
    char *editTarget = nullptr;
    size_t editTargetSize = 0;
    void (*editOnSaved)() = nullptr;
    // Fires once the editor closes, on EITHER exit path (saved or
    // canceled) -- unlike onSaved. Used by the Wi-Fi password step to
    // always attempt a reconnect on the way out, so canceling never just
    // leaves the radio silently disconnected (see runScan()'s comment).
    void (*editOnClosed)() = nullptr;

    void fireOnClosed()
    {
        if (!editOnClosed) return;
        void (*cb)() = editOnClosed;
        editOnClosed = nullptr;
        cb();
    }

    void editAccepted(const char *text)
    {
        if (editTarget && editTargetSize > 0)
        {
            strncpy(editTarget, text, editTargetSize - 1);
            editTarget[editTargetSize - 1] = '\0';
            Config::save();
            if (editOnSaved) editOnSaved();
        }
        fireOnClosed();
    }

    void editCancelled() { fireOnClosed(); }

    void openEditor(char *target, size_t targetSize, void (*onSaved)(), bool isPassword,
                    void (*onClosed)() = nullptr, const char *title = "Edit")
    {
        editTarget = target;
        editTargetSize = targetSize;
        editOnSaved = onSaved;
        editOnClosed = onClosed;
        RadialKeyboard::open(title, target, targetSize - 1, isPassword, editAccepted, editCancelled);
    }

    // A category's control panel: fills the whole face and scrolls
    // vertically, hidden until its category is opened.
    //
    // It used to be a 186px circle inset inside the 240px screen, which left
    // a wide dead margin and only ~142px of usable width. Filling the screen
    // and controlling the usable area with PADDING instead is worth ~40px
    // more width.
    //
    // The padding is what keeps content inside the round bezel, so it isn't
    // arbitrary: content spans y=46..184, and the panel's half-width at
    // those extremes is sqrt(120^2 - 74^2) = 94px, comfortably clear of the
    // 90px half-width the 180px content column needs. Widening the content
    // or shrinking the vertical padding will start clipping rows against the
    // curve at the top and bottom.
    lv_obj_t *makeCardShell(const char *title)
    {
        lv_obj_t *card = lv_obj_create(screenRoot);
        lv_obj_set_size(card, 240, 240);
        lv_obj_center(card);
        lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0); // the screen behind it already carries the background
        lv_obj_set_style_radius(card, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_pad_hor(card, 30, 0);
        lv_obj_set_style_pad_top(card, 46, 0);
        lv_obj_set_style_pad_bottom(card, 56, 0); // clears the back button
        lv_obj_set_style_pad_row(card, 8, 0);
        lv_obj_set_scroll_dir(card, LV_DIR_VER);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // No scrollbar. A straight bar down the edge of a CIRCULAR panel
        // can't hug anything -- it just cuts across the face and reads as a
        // rendering fault. Scrolling still works by knob and by drag; the
        // bar was only ever an indicator, and a misleading one here.
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

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
        openEditor(Config::get().wifiPass, sizeof(Config::get().wifiPass), nullptr, true, reconnectAfterWifiEdit, "Password");
    }

    void runScan()
    {
        lv_obj_clean(scanList);
        lv_label_set_text(scanStatusLbl, "Scanning...");
        // Force the redraw NOW: WiFi.scanNetworks() below blocks this task
        // for seconds, so without this the "Scanning..." label wouldn't
        // reach the panel until the scan had already finished -- the screen
        // just appeared frozen on its previous contents.
        lv_refr_now(NULL);
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
        lv_obj_set_style_bg_color(cancelBtn, Palette::bgSecondary(), 0);
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
        lv_obj_set_style_bg_color(rescanBtn, Palette::bgSecondary(), 0);
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
        openEditor(Config::get().wifiPass, sizeof(Config::get().wifiPass), nullptr, true, reconnectAfterWifiEdit, "Password");
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
        openEditor(Config::get().fluidNcHost, sizeof(Config::get().fluidNcHost), refreshHostLabels, false, nullptr, "FluidNC host");
    }

    void tpHostCb(lv_event_t *e)
    {
        (void)e;
        openEditor(Config::get().terraPixelHost, sizeof(Config::get().terraPixelHost), refreshHostLabels, false, nullptr, "LED host");
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
        // 0.5mm - 20.0mm, stored x10 so the slider can stay integer
        lv_obj_t *penMmSlider = uiMakeSlider(penMmRow, 5, 200, (int)(Config::get().penJogMm * 10.0f));
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
        lv_obj_t *penFeedSlider = uiMakeSlider(penFeedRow, 100, 3000, (int)Config::get().penJogFeed);
        lv_obj_add_event_cb(penFeedSlider, penFeedSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(penFeedSlider, penFeedSliderCb, LV_EVENT_RELEASED, NULL);
        {
            char buf[28];
            snprintf(buf, sizeof(buf), "Pen feed: %.0fmm/min", Config::get().penJogFeed);
            lv_label_set_text(penFeedLbl, buf);
        }

        return card;
    }

    // ---- Display card (brightness, menu direction, screen sleep) ----
    lv_obj_t *sleepLedLbl = nullptr;
    lv_obj_t *brightLbl = nullptr;

    void brightSliderCb(lv_event_t *e)
    {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        int v = lv_slider_get_value(slider);
        char buf[24];
        snprintf(buf, sizeof(buf), "Brightness: %d%%", v);
        lv_label_set_text(brightLbl, buf);

        // Apply live while dragging so the slider actually does something
        // visible -- this screen previously only had the auto-dim TIMEOUT
        // slider, which is why "the backlight slider" appeared to do nothing
        // to brightness.
        Config::get().backlightBrightnessPct = (uint8_t)v;
        backlightSet((uint8_t)v);

        if (lv_event_get_code(e) == LV_EVENT_RELEASED) Config::save();
    }

    void invertRotCb(lv_event_t *e)
    {
        lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
        Config::get().invertMenuRotation = lv_obj_has_state(sw, LV_STATE_CHECKED);
        Config::save();
    }

    // Sleep timeout as discrete chips rather than a slider: these are a few
    // named choices, not a continuum, and chips are a far easier touch
    // target than hitting an exact second on a 140px slider.
    const int SLEEP_OPTION_COUNT = 4;
    const uint16_t SLEEP_OPTION_SECS[SLEEP_OPTION_COUNT] = {0, 180, 300, 600};
    const char *SLEEP_OPTION_LABELS[SLEEP_OPTION_COUNT] = {"Never", "3m", "5m", "10m"};
    lv_obj_t *sleepChips[SLEEP_OPTION_COUNT] = {nullptr};
    lv_obj_t *sleepChipLbls[SLEEP_OPTION_COUNT] = {nullptr};

    void restyleSleepChips()
    {
        for (int i = 0; i < SLEEP_OPTION_COUNT; i++)
        {
            bool sel = Config::get().sleepTimeoutSec == SLEEP_OPTION_SECS[i];
            lv_obj_set_style_bg_color(sleepChips[i], sel ? Palette::accent() : Palette::bgPanel(), 0);
            lv_obj_set_style_text_color(sleepChipLbls[i], sel ? Palette::accentFg() : Palette::textMuted(), 0);
        }
    }

    void sleepChipCb(lv_event_t *e)
    {
        int i = (int)(intptr_t)lv_event_get_user_data(e);
        Config::get().sleepTimeoutSec = SLEEP_OPTION_SECS[i];
        Config::save();
        restyleSleepChips();
    }

    void sleepLedSliderCb(lv_event_t *e)
    {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        int v = lv_slider_get_value(slider);
        char buf[28];
        snprintf(buf, sizeof(buf), "Ring asleep: %d%%", v);
        lv_label_set_text(sleepLedLbl, buf);
        Config::get().sleepLedBrightnessPct = (uint8_t)v;
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) Config::save();
    }

    lv_obj_t *makeDisplayCard()
    {
        lv_obj_t *card = makeCardShell("DISPLAY");

        lv_obj_t *brightRow = uiMakeRow(card);
        brightLbl = lv_label_create(brightRow);
        lv_obj_set_style_text_font(brightLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(brightLbl, Palette::textMuted(), 0);
        // Floor of 10%: 0 would black the panel out with no way to see the
        // slider well enough to turn it back up.
        lv_obj_t *brightSlider = uiMakeSlider(brightRow, 10, 100, Config::get().backlightBrightnessPct);
        lv_obj_add_event_cb(brightSlider, brightSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(brightSlider, brightSliderCb, LV_EVENT_RELEASED, NULL);
        {
            char buf[24];
            snprintf(buf, sizeof(buf), "Brightness: %d%%", Config::get().backlightBrightnessPct);
            lv_label_set_text(brightLbl, buf);
        }

        lv_obj_t *invertRow = uiMakeRow(card, "Invert menu rotation");
        lv_obj_t *invertSw = uiMakeSwitch(invertRow, Config::get().invertMenuRotation);
        lv_obj_add_event_cb(invertSw, invertRotCb, LV_EVENT_VALUE_CHANGED, NULL);

        lv_obj_t *sleepRow = uiMakeRow(card, "Sleep after");
        lv_obj_t *chipRow = lv_obj_create(sleepRow);
        lv_obj_set_size(chipRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(chipRow, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(chipRow, 0, 0);
        lv_obj_set_style_pad_all(chipRow, 0, 0);
        lv_obj_set_style_pad_column(chipRow, 4, 0);
        lv_obj_clear_flag(chipRow, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(chipRow, LV_FLEX_FLOW_ROW);
        for (int i = 0; i < SLEEP_OPTION_COUNT; i++)
        {
            lv_obj_t *chip = lv_obj_create(chipRow);
            lv_obj_set_size(chip, 38, 26);
            lv_obj_set_style_radius(chip, 13, 0);
            lv_obj_set_style_border_width(chip, 0, 0);
            lv_obj_set_style_pad_all(chip, 0, 0);
            lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_ext_click_area(chip, 4);
            lv_obj_add_event_cb(chip, sleepChipCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            lv_obj_t *lbl = lv_label_create(chip);
            lv_label_set_text(lbl, SLEEP_OPTION_LABELS[i]);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            lv_obj_center(lbl);
            sleepChips[i] = chip;
            sleepChipLbls[i] = lbl;
        }
        restyleSleepChips();

        // The ring stays lit while the screen is off, so machine state is
        // still readable across the room mid-plot.
        lv_obj_t *sleepLedRow = uiMakeRow(card);
        sleepLedLbl = lv_label_create(sleepLedRow);
        lv_obj_set_style_text_font(sleepLedLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(sleepLedLbl, Palette::textMuted(), 0);
        lv_obj_t *sleepLedSlider = uiMakeSlider(sleepLedRow, 0, 100, Config::get().sleepLedBrightnessPct);
        lv_obj_add_event_cb(sleepLedSlider, sleepLedSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(sleepLedSlider, sleepLedSliderCb, LV_EVENT_RELEASED, NULL);
        {
            char buf[28];
            snprintf(buf, sizeof(buf), "Ring asleep: %d%%", Config::get().sleepLedBrightnessPct);
            lv_label_set_text(sleepLedLbl, buf);
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

    // ---- category ring ----
    void showRing()
    {
        if (openPanel >= 0) lv_obj_add_flag(panels[openPanel], LV_OBJ_FLAG_HIDDEN);
        openPanel = -1;
        ring.setVisible(true);
        lv_obj_clear_flag(hub, LV_OBJ_FLAG_HIDDEN);
    }

    void openCategory(int index)
    {
        if (index < 0 || index >= CATEGORY_COUNT) return;
        ring.setVisible(false);
        lv_obj_add_flag(hub, LV_OBJ_FLAG_HIDDEN);
        openPanel = index;
        lv_obj_clear_flag(panels[index], LV_OBJ_FLAG_HIDDEN);
        lv_obj_scroll_to_y(panels[index], 0, LV_ANIM_OFF);
    }

    void refreshHub(int index)
    {
        lv_label_set_text(hubNameLbl, CATEGORY_NAMES[index]);
    }

    void hubTapCb(lv_event_t *e)
    {
        (void)e;
        ring.openSelected();
    }

    void backBtnCb(lv_event_t *e)
    {
        (void)e;
        // Same button serves both levels: step out of a category first, and
        // only leave Settings once the ring is what's showing.
        if (openPanel >= 0) showRing();
        else UiNav::goHome();
    }

    void onItemStyle(lv_obj_t *chip, int, float nearness)
    {
        lv_opa_t mix = (lv_opa_t)(255 * nearness);
        lv_obj_set_style_bg_color(chip, lv_color_mix(Palette::accent(), Palette::bgSecondary(), mix), 0);
        lv_obj_t *icon = lv_obj_get_child(chip, 0);
        if (icon)
            lv_obj_set_style_text_color(icon, lv_color_mix(Palette::accentFg(), Palette::textMuted(), mix), 0);
    }

    lv_obj_t *makeChip(const char *icon)
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

        lv_obj_t *lbl = lv_label_create(chip);
        lv_label_set_text(lbl, icon);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_center(lbl);
        return chip;
    }
}

lv_obj_t *uiSettingsCreate()
{
    screenRoot = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screenRoot, Palette::bgApp(), 0);
    // The screen itself never scrolls -- everything is placed by hand -- so
    // suppress any scrollbar it might otherwise draw over the UI.
    lv_obj_clear_flag(screenRoot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screenRoot, LV_SCROLLBAR_MODE_OFF);

    // Panels first so the ring and hub end up drawn above them.
    panels[0] = makeWifiCard();
    panels[1] = makeMachineCard();
    panels[2] = makeDisplayCard();
    panels[3] = makeAboutCard();

    hub = lv_obj_create(screenRoot);
    lv_obj_set_size(hub, 82, 82);
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
    lv_obj_center(hub);

    hubNameLbl = lv_label_create(hub);
    lv_obj_set_style_text_font(hubNameLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hubNameLbl, Palette::text(), 0);
    lv_obj_align(hubNameLbl, LV_ALIGN_CENTER, 0, -8);

    hubHintLbl = lv_label_create(hub);
    lv_label_set_text(hubHintLbl, "open");
    lv_obj_set_style_text_font(hubHintLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hubHintLbl, Palette::accent(), 0);
    lv_obj_align(hubHintLbl, LV_ALIGN_CENTER, 0, 12);

    // Arc rather than a full circle, same reason as Jobs: on a full circle
    // four items land at 12/3/6/9 o'clock, and the 6 o'clock one sits right
    // on top of the back button below.
    //
    // 40-degree pitch keeps all four inside the +/-132 arc at every
    // selection, so nothing is ever hidden -- unlike Jobs, this is a short
    // fixed menu and you want to see the whole thing. opaFar stays at 110
    // (not transparent like Jobs) so the far item stays legible instead of
    // fading out at the arc edge.
    ring.create(screenRoot, 74, 56, 34, LV_OPA_COVER, 110);
    ring.setArcLayout(40.0f, 132.0f);
    ring.setOnOpen(openCategory);
    ring.setOnSelect(refreshHub);
    ring.setOnItemStyle(onItemStyle);
    for (int i = 0; i < CATEGORY_COUNT; i++) ring.addItem(makeChip(CATEGORY_ICONS[i]));

    lv_obj_move_foreground(hub);
    refreshHub(0);

    // Custom back button rather than addBackButton(): this one has to step
    // out of an open category before it leaves the screen.
    lv_obj_t *back = lv_btn_create(screenRoot);
    lv_obj_set_size(back, 36, 36);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(back, Palette::bgSecondary(), 0);
    lv_obj_set_ext_click_area(back, 10);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_event_cb(back, backBtnCb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *backLbl = lv_label_create(back);
    lv_label_set_text(backLbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(backLbl, Palette::textMuted(), 0);
    lv_obj_center(backLbl);

    return screenRoot;
}

void uiSettingsHandleRotate(int32_t delta)
{
    if (delta == 0) return;
    if (openPanel >= 0)
    {
        // Inside a category the knob scrolls its controls -- several of the
        // panels are taller than the round-safe area.
        lv_obj_scroll_by(panels[openPanel], 0, -delta * 24, LV_ANIM_ON);
        return;
    }
    for (int32_t i = 0; i < delta; i++) ring.selectNext();
    for (int32_t i = 0; i < -delta; i++) ring.selectPrev();
}

void uiSettingsHandleClick()
{
    if (openPanel >= 0) return; // controls inside a panel are touch-operated
    ring.openSelected();
}

bool uiSettingsHandleBack()
{
    if (openPanel < 0) return false;
    showRing();
    return true;
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
