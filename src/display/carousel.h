#pragma once
#include <lvgl.h>

// Paged card carousel: center card full-size/opaque, previous/next cards
// dimmed + shrunk, peeking near the circular screen's left/right edges
// (naturally cropped by the round bezel, no software clip needed);
// everything else hidden. A "n of N" index label tracks the selection.
//
// This widget owns paging mechanics and chrome only -- the caller creates
// each card's content (any lv_obj_t) and registers it via addCard();
// Carousel repositions/dims/hides it, it doesn't know what's inside. Cards
// MUST be created as children of the same `parent` passed to create(),
// since positioning uses lv_obj_align relative to that parent.
//
// Tapping a card fires onOpen only when it's already the center (selected)
// card -- peek cards aren't directly tappable to jump to, matching how
// slivers of a neighbor aren't a reliable touch target on a 240px round
// screen. A tap is delivered as an LVGL CLICKED event on the card object
// itself, so interactive children inside a card (sliders, buttons,
// textareas) still get their own taps normally -- LVGL doesn't bubble
// clicks from a clickable child up to its parent by default.
class Carousel
{
public:
    // centerSize/peekSize let a content-heavy carousel (e.g. Settings'
    // cards full of sliders) use a bigger center card than an icon-only
    // one (Home, Jobs) -- defaults match the mockup's icon-card proportion.
    void create(lv_obj_t *parent, lv_coord_t centerSize = 150, lv_coord_t peekSize = 84);

    // Registers a pre-built card, shown in the order added.
    void addCard(lv_obj_t *card);

    // Deletes every registered card (lv_obj_del) and resets to empty --
    // for screens whose card set is rebuilt at runtime (e.g. a file
    // listing), unlike a fixed set built once at create().
    void clear();

    void selectNext();
    void selectPrev();
    int selectedIndex() const { return selectedIndex_; }
    int count() const { return count_; }

    // Fires the onOpen callback for the currently-selected card -- call
    // this from a knob click.
    void openSelected();

    // Fired when the center card is opened (knob click, or a tap on it).
    void setOnOpen(void (*cb)(int index)) { onOpen_ = cb; }

private:
    static const int MAX_CARDS = 16;

    lv_obj_t *root_ = nullptr; // the parent every card lives under
    lv_obj_t *indexLbl_ = nullptr;
    lv_obj_t *cards_[MAX_CARDS] = {nullptr};
    int count_ = 0;
    int selectedIndex_ = 0;
    lv_coord_t centerSize_ = 150;
    lv_coord_t peekSize_ = 84;

    void (*onOpen_)(int index) = nullptr;

    void restyleCards();
    static void tapEventCb(lv_event_t *e);
};
