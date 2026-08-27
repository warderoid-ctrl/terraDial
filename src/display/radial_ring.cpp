#include "radial_ring.h"
#include <math.h>

namespace
{
    // Springy step, matching the mockup's "~550ms overshoot" brief but
    // shortened after testing -- the full 550 felt like the ring was
    // lagging behind the physical knob.
    const uint32_t SPIN_MS = 320;

    float normalizeAngle(float deg)
    {
        while (deg > 180.0f) deg -= 360.0f;
        while (deg < -180.0f) deg += 360.0f;
        return deg;
    }
}

void RadialRing::create(lv_obj_t *parent, lv_coord_t radius, lv_coord_t sizeNear,
                        lv_coord_t sizeFar, lv_opa_t opaNear, lv_opa_t opaFar)
{
    parent_ = parent;
    radius_ = radius;
    sizeNear_ = sizeNear;
    sizeFar_ = sizeFar;
    opaNear_ = opaNear;
    opaFar_ = opaFar;
    count_ = 0;
    selectedIndex_ = 0;
    offsetX100_ = 0;
    targetOffsetX100_ = 0;
}

void RadialRing::setArcLayout(float stepDeg, float halfArcDeg)
{
    fixedStepDeg_ = stepDeg;
    halfArcDeg_ = halfArcDeg;
    layout();
}

void RadialRing::setSpread(float amount)
{
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 0.95f) amount = 0.95f; // 1.0 would flatten the curve to zero slope at the bottom
    spread_ = amount;
    layout();
}

// u + a*sin(pi*u)/pi over u in [-1,1], scaled back out to degrees.
//
// Picked because it pins both ends: it maps 0 to 0 and +/-limit to
// +/-limit exactly, so the top slot stays at 12 o'clock and the far slot
// stays at the bottom however hard the curve is bent -- only the slots in
// between move. Its slope is (1 + a*cos(pi*u)), i.e. 1+a at the top and
// 1-a at the bottom: spacing is stretched around the selection and
// squeezed opposite it, monotonically, so items never reorder.
float RadialRing::spreadAngle(float angle, float limit) const
{
    if (spread_ <= 0.0f || limit <= 0.0f) return angle;
    float u = angle / limit;
    return limit * (u + spread_ * sinf((float)M_PI * u) / (float)M_PI);
}

void RadialRing::addItem(lv_obj_t *item)
{
    if (count_ >= MAX_ITEMS) return;
    items_[count_] = item;
    lv_obj_add_event_cb(item, itemTapCb, LV_EVENT_CLICKED, this);
    count_++;
    layout();
}

void RadialRing::clear()
{
    lv_anim_del(this, animCb);
    for (int i = 0; i < count_; i++)
    {
        if (items_[i]) lv_obj_del(items_[i]);
        items_[i] = nullptr;
    }
    count_ = 0;
    selectedIndex_ = 0;
    offsetX100_ = 0;
    targetOffsetX100_ = 0;
}

void RadialRing::setVisible(bool visible)
{
    visible_ = visible;
    if (!visible)
    {
        for (int i = 0; i < count_; i++)
            if (items_[i]) lv_obj_add_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    layout(); // restores per-item visibility according to the arc
}

void RadialRing::layout()
{
    // Hidden wholesale by the owner -- don't fight it by un-hiding items on
    // the next animation frame.
    if (!visible_ || count_ == 0) return;
    float offsetDeg = offsetX100_ / 100.0f;
    for (int i = 0; i < count_; i++)
    {
        // Full-circle mode wraps, so an item that rotates past the bottom
        // reappears on the other side. Arc mode must NOT wrap: with a fixed
        // pitch the list is usually longer than 360 degrees, and wrapping
        // would fold distant items back on top of nearby ones.
        float raw = i * stepDeg() + offsetDeg;
        float angle = arcMode() ? raw : normalizeAngle(raw);

        float limit = arcMode() ? halfArcDeg_ : 180.0f;
        if (arcMode() && fabsf(angle) >= limit)
        {
            // Outside the arc: hidden outright, so it costs nothing to draw
            // and can't be tapped. Tested on the even angle, so which items
            // the arc admits doesn't change with the spread.
            lv_obj_add_flag(items_[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(items_[i], LV_OBJ_FLAG_HIDDEN);

        angle = spreadAngle(angle, limit);

        // 1 at the top slot, 0 at the edge. Derived from the SPREAD angle,
        // not the even one, so size tracks where an item actually sits:
        // pushing the selection's neighbours further round the ring is what
        // shrinks them, which is what makes the top item stand out.
        float nearness = 1.0f - fabsf(angle) / limit;
        if (nearness < 0.0f) nearness = 0.0f;

        float rad = angle * (float)M_PI / 180.0f;
        lv_coord_t x = (lv_coord_t)(radius_ * sinf(rad));
        lv_coord_t y = (lv_coord_t)(-radius_ * cosf(rad));

        // Real size changes, NOT style transform_zoom: combining
        // transform_zoom with opa<255 makes LVGL render every item through
        // an offscreen layer each frame, which was unreliable on this
        // device's heap (items drew blank at rest and only flashed into
        // view mid-animation).
        lv_coord_t size = sizeFar_ + (lv_coord_t)((sizeNear_ - sizeFar_) * nearness);
        lv_obj_set_size(items_[i], size, size);
        lv_obj_align(items_[i], LV_ALIGN_CENTER, x, y);
        lv_obj_set_style_opa(items_[i], (lv_opa_t)(opaFar_ + (lv_opa_t)((opaNear_ - opaFar_) * nearness)), 0);

        if (onItemStyle_) onItemStyle_(items_[i], i, nearness);

        nearness_[i] = nearness;
    }

    // Stack nearer items over further ones. A plain "selected to the front"
    // isn't enough once spread_ bunches the far side up: down there chips
    // overlap, and without an explicit order the one in front is whichever
    // happens to sit later in the child list, which flips as the ring turns.
    //
    // Insertion sort on indices rather than a general sort: the list is
    // tiny, already almost ordered (nearness varies smoothly around the
    // ring), and this runs on every animation frame.
    int order[MAX_ITEMS];
    int n = 0;
    for (int i = 0; i < count_; i++)
    {
        if (lv_obj_has_flag(items_[i], LV_OBJ_FLAG_HIDDEN)) continue;
        int j = n++;
        while (j > 0 && nearness_[order[j - 1]] > nearness_[i])
        {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
    }
    for (int k = 0; k < n; k++) lv_obj_move_foreground(items_[order[k]]);
}

void RadialRing::animCb(void *inst, int32_t v)
{
    RadialRing *self = (RadialRing *)inst;
    self->offsetX100_ = v;
    self->layout();
}

void RadialRing::animateTo(int32_t targetX100)
{
    targetOffsetX100_ = targetX100;
    lv_anim_del(this, animCb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, this);
    lv_anim_set_exec_cb(&a, animCb);
    lv_anim_set_values(&a, offsetX100_, targetX100);
    lv_anim_set_time(&a, SPIN_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
}

void RadialRing::stepSelection(int delta)
{
    if (count_ == 0) return;

    int prev = selectedIndex_;
    if (arcMode())
    {
        // A list, not a loop: stop at the ends rather than jumping from the
        // last job back to the first.
        int next = selectedIndex_ + delta;
        if (next < 0) next = 0;
        if (next > count_ - 1) next = count_ - 1;
        selectedIndex_ = next;
        if (selectedIndex_ == prev) return; // already at an end -- nothing moves
        animateTo(-(int32_t)(selectedIndex_ * stepDeg() * 100));
    }
    else
    {
        selectedIndex_ = ((selectedIndex_ + delta) % count_ + count_) % count_;
        animateTo(targetOffsetX100_ - (int32_t)(delta * stepDeg() * 100));
    }
    if (onSelect_) onSelect_(selectedIndex_);
}

void RadialRing::selectNext() { stepSelection(1); }
void RadialRing::selectPrev() { stepSelection(-1); }

void RadialRing::openSelected()
{
    if (count_ > 0 && onOpen_) onOpen_(selectedIndex_);
}

void RadialRing::itemTapCb(lv_event_t *e)
{
    RadialRing *self = (RadialRing *)lv_event_get_user_data(e);
    lv_obj_t *target = lv_event_get_target(e);
    for (int i = 0; i < self->count_; i++)
    {
        if (self->items_[i] != target) continue;

        if (i != self->selectedIndex_)
        {
            // Bring the tapped item to the top. This doesn't block opening --
            // touch is a direct-select, so the tap opens it too.
            if (self->arcMode())
            {
                self->selectedIndex_ = i;
                self->animateTo(-(int32_t)(i * self->stepDeg() * 100));
            }
            else
            {
                // Full circle: go the shorter way round.
                int fwd = (i - self->selectedIndex_ + self->count_) % self->count_;
                int back = self->count_ - fwd;
                int32_t stepsX100 = (fwd <= back) ? (int32_t)(-fwd * self->stepDeg() * 100)
                                                  : (int32_t)(back * self->stepDeg() * 100);
                self->selectedIndex_ = i;
                self->animateTo(self->targetOffsetX100_ + stepsX100);
            }
            if (self->onSelect_) self->onSelect_(i);
        }
        if (self->onOpen_) self->onOpen_(i);
        return;
    }
}
