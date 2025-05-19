#pragma once

#include <stdint.h>
#include "hw/device.h"
#include "hw/pio.h"

typedef struct {
} zvb_crc32_t;

int zvb_crc32_init(zvb_crc32_t* crc32);
void zvb_crc32_write(zvb_crc32_t* crc32, uint16_t subaddr, uint8_t data);
uint8_t zvb_crc32_read(zvb_crc32_t* crc32, uint16_t subaddr);
