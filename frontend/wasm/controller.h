#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "snes_adapter.h"

void snes_controller_init(snes_controller_t* ctrl, uint8_t index);
bool snes_controller_available(uint8_t index);
const char* snes_controller_name(uint8_t index);
uint16_t snes_controller_latch(snes_controller_t* ctrl);
void snes_controller_event(const SDL_Event* event);
void snes_controller_shutdown(void);
