/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdio.h>
#include "hw/flash.h"
#include "hw/compactflash.h"
#include "hw/i2c/at24c512.h"
#include "hw/i2c/ds1307.h"
#include "hw/zvb/zvb_spi.h"

typedef struct {
    at24c512_t* eeprom_device;
    zvb_spi_t* tf_device;
    compactflash_t* cf_device;
    FILE* eeprom;
    FILE* tf;
    int cf_fd;
    char* cf_name;
} frontend_media_t;
extern frontend_media_t frontend_media;

int flash_load_from_file(flash_t*, const char* rom, const char* userprog);
int flash_save_to_file(flash_t*, const char* path);
/**
 * @brief Initialize the EEPROM and load the supplied backing file, if any.
 */
int frontend_eeprom_init(frontend_media_t*, at24c512_t*, const char* path);
/**
 * @brief Close the EEPROM backing file. Completed pages are flushed at I2C STOP;
 * this also flushes any remaining stdio data.
 */
void frontend_eeprom_close(frontend_media_t*);
void frontend_tf_close(frontend_media_t*);
int frontend_cf_init(frontend_media_t*, compactflash_t*, const char* path);
/**
 * @brief Load an image for the TF card; return zero on success, one on error.
 */
int frontend_tf_load(frontend_media_t*, zvb_spi_t*, const char* path);
void frontend_rtc_bind(ds1307_t*);

void frontend_media_close(frontend_media_t*);
