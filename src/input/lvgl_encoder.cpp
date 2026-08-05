#include "lvgl_encoder.h"
#include "encoder.h"

namespace
{
    lv_indev_t *indev = nullptr;
    bool captured = false;
    void (*cancelCb)() = nullptr;
    void asyncCancel(void *);

    // A knob Click arrives as a single discrete event, but LVGL's encoder
    // model wants a press state it can observe going down and then up. Latch
    // one event into exactly that two-read sequence.
    bool pendingPress = false;
    bool holdingPress = false;

    // The cancel callback tears down the very widgets LVGL is reading input
    // for, so it must not run inside the read callback itself. lv_async_call
    // defers it to a safe point between LVGL operations.
    void asyncCancel(void *)
    {
        void (*cb)() = cancelCb;
        cancelCb = nullptr;
        if (cb) cb();
    }

    void encoderRead(lv_indev_drv_t *drv, lv_indev_data_t *data)
    {
        (void)drv;
        data->enc_diff = 0;
        data->state = LV_INDEV_STATE_REL;
        if (!captured) return; // ui_nav owns the knob right now

        data->enc_diff = (int16_t)jogWheel.takeRotationDelta();

        ButtonEvent ev = jogWheel.takeButtonEvent();
        if (ev == ButtonEvent::LongPress)
        {
            if (cancelCb) lv_async_call(asyncCancel, nullptr);
            return;
        }
        if (ev == ButtonEvent::Click || ev == ButtonEvent::DoubleClick) pendingPress = true;

        if (pendingPress)
        {
            data->state = LV_INDEV_STATE_PR;
            pendingPress = false;
            holdingPress = true;
        }
        else if (holdingPress)
        {
            data->state = LV_INDEV_STATE_REL; // completes the press->release pair
            holdingPress = false;
        }
    }
}

namespace LvglEncoder
{
    void begin()
    {
        static lv_indev_drv_t drv;
        lv_indev_drv_init(&drv);
        drv.type = LV_INDEV_TYPE_ENCODER;
        drv.read_cb = encoderRead;
        indev = lv_indev_drv_register(&drv);
    }

    void capture(lv_group_t *group, void (*onCancel)())
    {
        if (!indev) return;
        cancelCb = onCancel;
        pendingPress = false;
        holdingPress = false;
        lv_indev_set_group(indev, group);
        captured = true;
    }

    void release()
    {
        if (!indev) return;
        captured = false;
        cancelCb = nullptr;
        lv_indev_set_group(indev, nullptr);
    }

    bool isCaptured() { return captured; }
}
