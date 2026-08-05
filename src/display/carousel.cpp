#include "carousel.h"
#include "palette.h"
#include <stdio.h>

namespace
{
    // Proportions adapted from the "TerraPen Dial UI" mockup's carousel
    // screens (Radial dial menu mockups/design_handoff_radial_dial_ui/) to
    // this project's real 240x240 round panel rather than its 440px mockup
    // canvas -- treat as a starting point to nudge on hardware, not a
    // pixel-exact port.
    const lv_coord_t PEEK_X_OFFSET = 92;
    const lv_coord_t CARD_Y_OFFSET = -10; // nudge up, leaving room for the index label below
    const lv_coord_t INDEX_Y_OFFSET = 74; // was 96 -- leaves room for a back button below it (ui_screen_shell.h's addBackButton())
}

void Carousel::create(lv_obj_t *parent, lv_coord_t centerSize, lv_coord_t peekSize)
{
    root_ = parent;
    centerSize_ = centerSize;
    peekSize_ = peekSize;

    indexLbl_ = lv_label_create(parent);
    lv_obj_set_style_text_font(indexLbl_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(indexLbl_, Palette::border(), 0);
    lv_obj_align(indexLbl_, LV_ALIGN_CENTER, 0, INDEX_Y_OFFSET);
}

void Carousel::addCard(lv_obj_t *card)
{
    if (count_ >= MAX_CARDS) return;
    cards_[count_] = card;
    lv_obj_add_event_cb(card, tapEventCb, LV_EVENT_CLICKED, this);
    count_++;
    restyleCards();
}

void Carousel::clear()
{
    for (int i = 0; i < count_; i++) lv_obj_del(cards_[i]);
    count_ = 0;
    selectedIndex_ = 0;
    lv_label_set_text(indexLbl_, "");
}

void Carousel::selectNext()
{
    if (count_ == 0) return;
    selectedIndex_ = (selectedIndex_ + 1) % count_;
    restyleCards();
}

void Carousel::selectPrev()
{
    if (count_ == 0) return;
    selectedIndex_ = (selectedIndex_ - 1 + count_) % count_;
    restyleCards();
}

void Carousel::openSelected()
{
    if (onOpen_) onOpen_(selectedIndex_);
}

void Carousel::restyleCards()
{
    int prev = (selectedIndex_ - 1 + count_) % count_;
    int next = (selectedIndex_ + 1) % count_;

    for (int i = 0; i < count_; i++)
    {
        lv_obj_t *c = cards_[i];
        if (i == selectedIndex_)
        {
            lv_obj_clear_flag(c, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(c, centerSize_, centerSize_);
            lv_obj_align(c, LV_ALIGN_CENTER, 0, CARD_Y_OFFSET);
            lv_obj_set_style_opa(c, LV_OPA_COVER, 0);
            lv_obj_move_foreground(c);
        }
        else if (i == prev && count_ > 1)
        {
            lv_obj_clear_flag(c, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(c, peekSize_, peekSize_);
            lv_obj_align(c, LV_ALIGN_CENTER, -PEEK_X_OFFSET, CARD_Y_OFFSET);
            lv_obj_set_style_opa(c, LV_OPA_50, 0);
        }
        else if (i == next && count_ > 1)
        {
            lv_obj_clear_flag(c, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(c, peekSize_, peekSize_);
            lv_obj_align(c, LV_ALIGN_CENTER, PEEK_X_OFFSET, CARD_Y_OFFSET);
            lv_obj_set_style_opa(c, LV_OPA_50, 0);
        }
        else
        {
            lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
        }
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d of %d", selectedIndex_ + 1, count_);
    lv_label_set_text(indexLbl_, buf);
}

void Carousel::tapEventCb(lv_event_t *e)
{
    Carousel *self = (Carousel *)lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target(e);
    for (int i = 0; i < self->count_; i++)
    {
        if (self->cards_[i] == target)
        {
            if (i == self->selectedIndex_ && self->onOpen_) self->onOpen_(i);
            return;
        }
    }
}
