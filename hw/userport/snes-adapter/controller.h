#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hw/userport/snes_adapter.h"

void snes_controller_init(snes_controller_t* ctrl, uint8_t index);
void snes_controller_load_mappings(void);
bool snes_controller_available(uint8_t index);
const char* snes_controller_name(uint8_t index);
uint16_t snes_controller_latch(snes_controller_t* ctrl);
