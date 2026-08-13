#include "radial_keyboard.h"
#include "palette.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

namespace
{
    // ---- key model ----
    // Everything on the rim is a Key. Printable keys append their character;
    // action keys do something to the buffer or the overlay.
    enum class Action : uint8_t
    {
        Char,
        Space,
        Backspace,
        NextPage,
        Accept,
    };

    struct Key
    {
        const char *label; // what's drawn on the rim
        char ch;           // the character appended, for Action::Char
        Action action;
    };

    // Three pages, cycled by the "abc/ABC/123" key. Splitting them keeps any
    // one ring sparse enough that neighbouring keys don't collide visually.
    const char *PAGE_CHARS[] = {
        "abcdefghijklmnopqrstuvwxyz",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        "0123456789.-_@#$%&*+=/:;,!?()",
    };
    const char *PAGE_NEXT_LABEL[] = {"ABC", "123", "abc"};
    const int PAGE_COUNT = 3;

    const int MAX_KEYS = 40;

    // Rim geometry. RADIUS keeps the labels inside the round panel's ~120px
    // visible circle with margin for the enlarged selected glyph.
    const lv_coord_t RADIUS = 98;
    const lv_coord_t HUB_SIZE = 104;

    lv_obj_t *overlay = nullptr;
    lv_obj_t *hub = nullptr;
    lv_obj_t *titleLbl = nullptr;
    lv_obj_t *textLbl = nullptr;
    lv_obj_t *previewLbl = nullptr;
    lv_obj_t *keyLbls[MAX_KEYS] = {nullptr};

    Key keys[MAX_KEYS];
    int keyCount = 0;
    int selected = 0;
    int prevSelected = -1;
    int page = 0;

    char buffer[80];
    size_t bufMax = sizeof(buffer) - 1;
    bool isPassword = false;
    char titleText[24];

    void (*acceptCb)(const char *) = nullptr;
    void (*cancelCb)() = nullptr;

    void refreshText()
    {
        if (isPassword)
        {
            // Masked, but still show the length so it's obvious typing is
            // landing -- a blank field with no feedback is worse than none.
            char masked[sizeof(buffer)];
            size_t n = strlen(buffer);
            if (n > bufMax) n = bufMax;
            for (size_t i = 0; i < n; i++) masked[i] = '*';
            masked[n] = '\0';
            lv_label_set_text(textLbl, n ? masked : "(empty)");
        }
        else
        {
            lv_label_set_text(textLbl, buffer[0] ? buffer : "(empty)");
        }
    }

    void refreshPreview()
    {
        const Key &k = keys[selected];
        char buf[24];
        switch (k.action)
        {
            case Action::Space:     snprintf(buf, sizeof(buf), "space"); break;
            case Action::Backspace: snprintf(buf, sizeof(buf), "delete"); break;
            case Action::NextPage:  snprintf(buf, sizeof(buf), "%s", PAGE_NEXT_LABEL[page]); break;
            case Action::Accept:    snprintf(buf, sizeof(buf), "save"); break;
            case Action::Char:
            default:                snprintf(buf, sizeof(buf), "%c", k.ch); break;
        }
        lv_label_set_text(previewLbl, buf);
    }

    void styleKey(int i, bool sel)
    {
        if (i < 0 || i >= keyCount || !keyLbls[i]) return;
        lv_obj_set_style_text_font(keyLbls[i], sel ? &lv_font_montserrat_24 : &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(keyLbls[i], sel ? Palette::accent() : Palette::textMuted(), 0);
    }

    void applySelection()
    {
        // Only the two keys that changed state are restyled. Font changes
        // force a label relayout, so touching all ~30 every detent would be
        // needlessly expensive.
        if (prevSelected != selected)
        {
            styleKey(prevSelected, false);
            styleKey(selected, true);
            prevSelected = selected;
        }
        refreshPreview();
    }

    void keyTapCb(lv_event_t *e)
    {
        // Tapping a rim key only MOVES the highlight; the hub commits it.
        // At ~20px apart a mis-tap is likely, and a mis-tap that merely
        // moves the cursor costs nothing, whereas one that typed a wrong
        // character would.
        selected = (int)(intptr_t)lv_event_get_user_data(e);
        applySelection();
    }

    void buildRing()
    {
        for (int i = 0; i < keyCount; i++)
        {
            if (keyLbls[i]) lv_obj_del(keyLbls[i]);
            keyLbls[i] = nullptr;
        }

        keyCount = 0;
        const char *chars = PAGE_CHARS[page];
        for (const char *c = chars; *c && keyCount < MAX_KEYS - 4; c++)
        {
            keys[keyCount].label = nullptr;
            keys[keyCount].ch = *c;
            keys[keyCount].action = Action::Char;
            keyCount++;
        }
        keys[keyCount++] = {PAGE_NEXT_LABEL[page], 0, Action::NextPage};
        keys[keyCount++] = {"SP", ' ', Action::Space};
        keys[keyCount++] = {LV_SYMBOL_BACKSPACE, 0, Action::Backspace};
        keys[keyCount++] = {LV_SYMBOL_OK, 0, Action::Accept};

        for (int i = 0; i < keyCount; i++)
        {
            lv_obj_t *lbl = lv_label_create(overlay);
            char txt[8];
            if (keys[i].action == Action::Char) snprintf(txt, sizeof(txt), "%c", keys[i].ch);
            else snprintf(txt, sizeof(txt), "%s", keys[i].label);
            lv_label_set_text(lbl, txt);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(lbl, Palette::textMuted(), 0);

            // Keys sit at fixed angles and the HIGHLIGHT moves, rather than
            // the whole ring rotating like the home dial. Spinning ~30
            // labels every animation frame would cost far more than the
            // dial's 8, and with this many keys the rotation wouldn't read
            // as motion anyway.
            float angle = -90.0f + 360.0f * i / keyCount;
            float rad = angle * (float)M_PI / 180.0f;
            lv_obj_align(lbl, LV_ALIGN_CENTER,
                         (lv_coord_t)(RADIUS * cosf(rad)),
                         (lv_coord_t)(RADIUS * sinf(rad)));

            lv_obj_add_flag(lbl, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_ext_click_area(lbl, 8);
            lv_obj_add_event_cb(lbl, keyTapCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            keyLbls[i] = lbl;
        }

        if (selected >= keyCount) selected = 0;
        prevSelected = -1;
        styleKey(selected, true);
        prevSelected = selected;
        refreshPreview();
        lv_obj_move_foreground(hub); // keys are created after the hub, so re-raise it
    }

    void closeOverlay()
    {
        if (!overlay) return;
        lv_obj_del(overlay);
        overlay = nullptr;
        hub = titleLbl = textLbl = previewLbl = nullptr;
        for (int i = 0; i < MAX_KEYS; i++) keyLbls[i] = nullptr;
        keyCount = 0;
    }

    void activateSelected()
    {
        const Key &k = keys[selected];
        switch (k.action)
        {
            case Action::Char:
            case Action::Space:
            {
                size_t n = strlen(buffer);
                if (n < bufMax)
                {
                    buffer[n] = (k.action == Action::Space) ? ' ' : k.ch;
                    buffer[n + 1] = '\0';
                    refreshText();
                }
                break;
            }
            case Action::Backspace:
            {
                size_t n = strlen(buffer);
                if (n > 0)
                {
                    buffer[n - 1] = '\0';
                    refreshText();
                }
                break;
            }
            case Action::NextPage:
                page = (page + 1) % PAGE_COUNT;
                buildRing();
                break;
            case Action::Accept:
            {
                void (*cb)(const char *) = acceptCb;
                acceptCb = nullptr;
                cancelCb = nullptr;
                char finished[sizeof(buffer)];
                strncpy(finished, buffer, sizeof(finished) - 1);
                finished[sizeof(finished) - 1] = '\0';
                closeOverlay();
                if (cb) cb(finished);
                break;
            }
        }
    }

    void hubTapCb(lv_event_t *e)
    {
        (void)e;
        activateSelected();
    }
}

namespace RadialKeyboard
{
    void open(const char *title, const char *initial, size_t maxLen, bool password,
              void (*onAccept)(const char *), void (*onCancel)())
    {
        closeOverlay();

        acceptCb = onAccept;
        cancelCb = onCancel;
        isPassword = password;
        bufMax = (maxLen && maxLen < sizeof(buffer)) ? maxLen : sizeof(buffer) - 1;
        strncpy(buffer, initial ? initial : "", sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        strncpy(titleText, title ? title : "", sizeof(titleText) - 1);
        titleText[sizeof(titleText) - 1] = '\0';
        page = 0;
        selected = 0;
        prevSelected = -1;

        overlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(overlay, 240, 240);
        lv_obj_center(overlay);
        lv_obj_set_style_bg_color(overlay, Palette::bgApp(), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(overlay, 0, 0);
        lv_obj_set_style_radius(overlay, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

        // Centre hub: shows what you're editing and commits the highlighted
        // key. Same styling as the home dial's hub so the gesture reads the
        // same way -- big central target, always in the same place.
        hub = lv_obj_create(overlay);
        lv_obj_set_size(hub, HUB_SIZE, HUB_SIZE);
        lv_obj_center(hub);
        lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(hub, Palette::bgSecondary(), 0);
        lv_obj_set_style_bg_color(hub, Palette::accent(), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(hub, Palette::border(), 0);
        lv_obj_set_style_border_width(hub, 1, 0);
        lv_obj_set_style_pad_all(hub, 0, 0);
        lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(hub, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(hub, hubTapCb, LV_EVENT_CLICKED, NULL);

        titleLbl = lv_label_create(hub);
        lv_label_set_text(titleLbl, titleText);
        lv_obj_set_style_text_font(titleLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(titleLbl, Palette::textMuted(), 0);
        lv_obj_align(titleLbl, LV_ALIGN_CENTER, 0, -30);

        textLbl = lv_label_create(hub);
        lv_obj_set_width(textLbl, HUB_SIZE - 16);
        lv_label_set_long_mode(textLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(textLbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(textLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(textLbl, Palette::text(), 0);
        lv_obj_align(textLbl, LV_ALIGN_CENTER, 0, -8);

        // Restates the highlighted key in the middle, so you never have to
        // read the small rim glyph to know what a click will type.
        previewLbl = lv_label_create(hub);
        lv_obj_set_style_text_font(previewLbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(previewLbl, Palette::accent(), 0);
        lv_obj_align(previewLbl, LV_ALIGN_CENTER, 0, 24);

        refreshText();
        buildRing();
    }

    bool isOpen() { return overlay != nullptr; }

    void handleRotate(int32_t delta)
    {
        if (!overlay || keyCount == 0 || delta == 0) return;
        selected = (int)(((selected + delta) % keyCount + keyCount) % keyCount);
        applySelection();
    }

    void handleClick()
    {
        if (!overlay) return;
        activateSelected();
    }

    void handleLongPress()
    {
        if (!overlay) return;
        void (*cb)() = cancelCb;
        acceptCb = nullptr;
        cancelCb = nullptr;
        closeOverlay();
        if (cb) cb();
    }
}
