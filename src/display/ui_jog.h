#pragma once
#include <lvgl.h>
#include "../net/fluidnc_client.h"

lv_obj_t *uiJogCreate();

// Visual feedback for whether the knob currently controls jogging (vs.
// top-level screen navigation) -- driven by ui_nav's focus state machine.
void uiJogSetFocused(bool focused);

// Called by ui_nav only while this screen is focused.
void uiJogHandleRotate(int32_t delta);
void uiJogCycleStep();
void uiJogCycleAxis();

void uiJogUpdate(const FluidNCStatus &st);
