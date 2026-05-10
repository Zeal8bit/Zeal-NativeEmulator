#pragma once

#include <stdint.h>

#include "hw/userport/snes_adapter.h"

void snes_mouse_init(snes_mouse_t* mouse);
void snes_mouse_detach(snes_mouse_t* mouse);
void snes_mouse_reset_scale(snes_mouse_t* mouse);
uint32_t snes_mouse_latch(snes_mouse_t* mouse);
