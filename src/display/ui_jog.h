#pragma once
#include <lvgl.h>
#include "../net/fluidnc_client.h"

lv_obj_t *uiJogCreate();

// Called by ui_nav only while this screen is active.
void uiJogHandleRotate(int32_t delta); // knob rotate: continuous jog on the selected axis
void uiJogCycleAxis();                 // knob click: cycle X -> Y -> Z -> X

void uiJogUpdate(const FluidNCStatus &st);
