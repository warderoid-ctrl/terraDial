#include "ui_estop.h"
#include "../net/fluidnc_client.h"
#include "palette.h"
#include "ui_screen_shell.h"

// Always reachable as a dial destination, one button, no confirm dialog --
// a confirm step would defeat the point of an emergency stop. The mockup
// also wants this reachable via a double-press on a separate physical
// touch button "from anywhere"; that hardware doesn't exist on terraDial
// (only the touchscreen and the encoder's own click/double-click/long-
// press -- see pins.h/encoder.h), so this is reachable via the dial only.
namespace
{
    lv_obj_t *estopBtn = nullptr;
    lv_obj_t *nameLbl = nullptr;
    lv_obj_t *subLbl = nullptr;

    // How long the button holds its acknowledgement before returning to the
    // armed face. Long enough to read after you've looked back up at the
    // machine, short enough that the button is ready again if the first stop
    // didn't do it.
    const uint32_t ACK_MS = 5000;

    uint32_t firedAt = 0;
    bool ackShowing = false;

    void showArmed()
    {
        ackShowing = false;
        if (!estopBtn) return;
        lv_obj_set_style_bg_color(estopBtn, Palette::alert(), 0);
        lv_label_set_text(nameLbl, "E-STOP");
        lv_label_set_text(subLbl, "Feed hold\n+ soft reset");
    }

    // Two different acknowledgements, because they mean opposite things.
    //
    // The stop is fire-and-forget over a WebSocket: nothing comes back to
    // say the machine got it. What we CAN tell the user is whether there
    // was a connection to send it down -- and that distinction matters more
    // here than anywhere else in the firmware. Someone who jabs this and
    // sees a calm acknowledgement will believe the machine is stopping; if
    // the panel was actually off-network, they need to know to go and pull
    // the plug instead, immediately, not after watching the plot continue.
    void showAck(bool sent)
    {
        ackShowing = true;
        firedAt = millis();
        if (!estopBtn) return;
        if (sent)
        {
            lv_obj_set_style_bg_color(estopBtn, Palette::bgSecondary(), 0);
            lv_label_set_text(nameLbl, "STOPPED");
            lv_label_set_text(subLbl, "Hold + reset\nsent");
        }
        else
        {
            lv_obj_set_style_bg_color(estopBtn, Palette::bgSecondary(), 0);
            lv_label_set_text(nameLbl, "NOT SENT");
            lv_label_set_text(subLbl, "No connection --\nstop it by hand");
        }
    }

    // Fires on PRESS, not on click.
    //
    // LV_EVENT_CLICKED needs the touch to go down AND come back up inside
    // the button. That is the right contract for an ordinary control -- it
    // is what lets you slide off a button you didn't mean to hit -- and it
    // is the wrong one here: the gesture this button actually receives is a
    // panicked jab at a 1.28" screen, often with the other hand already
    // reaching for the machine, and a jab that lands and drags off is
    // exactly the input CLICKED is designed to discard. Pressing is the
    // whole of the intent; there is nothing to reconsider on the way up.
    void estopPressCb(lv_event_t *e)
    {
        (void)e;
        uiEstopTrigger();
    }
}

lv_obj_t *uiEstopCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);

    // Button sized to leave the back button clear at the bottom of the
    // circle. The "feed hold + soft reset" caption lives INSIDE the button
    // (as in the mockup): as a sibling anchored to the screen bottom it
    // landed on top of the red circle, since a 172px circle centered on a
    // 240px screen already reaches down to y=206.
    estopBtn = lv_btn_create(scr);
    lv_obj_set_size(estopBtn, 156, 156);
    lv_obj_set_style_radius(estopBtn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(estopBtn, Palette::alert(), 0);
    // Lights up under the finger. Without it the only sign the panel felt
    // the touch at all was the machine stopping -- which is the one piece of
    // feedback you cannot see while you are looking at the screen.
    lv_obj_set_style_bg_color(estopBtn, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_align(estopBtn, LV_ALIGN_CENTER, 0, -14);
    lv_obj_add_event_cb(estopBtn, estopPressCb, LV_EVENT_PRESSED, NULL);

    lv_obj_t *iconLbl = lv_label_create(estopBtn);
    lv_label_set_text(iconLbl, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(iconLbl, lv_color_white(), 0);
    lv_obj_align(iconLbl, LV_ALIGN_CENTER, 0, -30);

    nameLbl = lv_label_create(estopBtn);
    lv_label_set_text(nameLbl, "E-STOP");
    lv_obj_set_style_text_font(nameLbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(nameLbl, lv_color_white(), 0);
    lv_obj_align(nameLbl, LV_ALIGN_CENTER, 0, -2);

    subLbl = lv_label_create(estopBtn);
    lv_label_set_text(subLbl, "Feed hold\n+ soft reset");
    lv_obj_set_style_text_align(subLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(subLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subLbl, lv_color_white(), 0);
    lv_obj_set_style_text_opa(subLbl, LV_OPA_80, 0);
    lv_obj_align(subLbl, LV_ALIGN_CENTER, 0, 32);

    addBackButton(scr);

    return scr;
}

void uiEstopTrigger()
{
    // Read the connection BEFORE sending, so the acknowledgement describes
    // this stop rather than whatever the socket is doing a moment later.
    bool sent = fluidNC.status().connected;

    // Sent regardless. If the socket has dropped but not yet been noticed,
    // these still cost nothing and might still land.
    fluidNC.feedHold();
    fluidNC.softReset();

    showAck(sent);
}

void uiEstopUpdate()
{
    if (ackShowing && millis() - firedAt >= ACK_MS) showArmed();
}
