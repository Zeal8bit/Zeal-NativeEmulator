/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "hw/flash.h"
#include "hw/host.h"

typedef enum {
    STATE_IDLE,
    STATE_SOFTWARE_ID,
    /* State reached after *(0x5555)=0xAA operation is detected.
     * This operation is common to all erase, software id and write commands */
    STATE_SPECIAL_STEP0,
    STATE_SPECIAL_STEP1,    /* Similarly, with *(0x2AAAA)=0x55 */
    STATE_SPECIAL_STEP2,    /* Similarly, only relevant for erase command */
    STATE_SPECIAL_STEP3,    /* Similarly, only relevant for erase command */
    /* In this state, writing a byte is allowed, then a delay is applied */
    STATE_PERFORM_WRITE,
    /* During this state, the flash is being written */
    STATE_PERFORM_WRITE_DELAY,
    /* Ready to erase a block/sector */
    STATE_PERFORM_ERASE,
    /* Erasing a sector, delaying */
    STATE_PERFORM_ERASE_DELAY,
} fsm_state_t;

static uint8_t flash_read(device_t* device, uint32_t address)
{
    flash_t* flash = (flash_t*) device;

    switch (flash->state) {
        case STATE_PERFORM_ERASE_DELAY:
            return 0xff;
        case STATE_PERFORM_WRITE_DELAY: {
            /* DQ7 has already been flipped before programming the write */
            const uint8_t read_value = flash->writing_byte;
            /* Bit 6 must be flipped here */
            flash->writing_byte ^= 0x40;
            return read_value;
        }
        case STATE_SOFTWARE_ID:
            /**
             * On address 0, return SST Manufacturer's ID: 0xBF
             * On address 1, return the Device ID: 0xB6 (SST39SF020)
             */
            if (address == 0) return 0xBF;
            if (address == 1) return 0xB6;
            /* TODO: Verify on real hardware the behavior here */
            return 0xFF;
        default:
            break;
    }

    if (address >= flash->size) {
        hw_log(flash->parent.host, true, 0, "[FLASH] Invalid read size: %08x\n", address);
        return 0;
    }
    return flash->data[address];
}

static uint8_t flash_debug_read(device_t* device, uint32_t address)
{
    flash_t* flash = (flash_t*) device;
    if (address >= flash->size) {
        hw_log(flash->parent.host, true, 0, "[FLASH] Invalid debug read size: %08x\n", address);
        return 0;
    }
    return flash->data[address];
}

static void flash_write(device_t* device, uint32_t address, uint8_t data)
{
    flash_t* flash = (flash_t*) device;

    switch (flash->state) {
        case STATE_IDLE:
            if (address == 0x5555 && data == 0xaa) {
                flash->state = STATE_SPECIAL_STEP0;
            }
            break;
        case STATE_SPECIAL_STEP0:
            if (address == 0x2aaa && data == 0x55) {
                flash->state = STATE_SPECIAL_STEP1;
            }
            break;
        case STATE_SPECIAL_STEP1:
            if (address == 0x5555 && data == 0x90) {
                flash->state = STATE_SOFTWARE_ID;
            } else if (address == 0x5555 && data == 0xa0) {
                flash->state = STATE_PERFORM_WRITE;
            } else if (address == 0x5555 && data == 0x80) {
                flash->state = STATE_SPECIAL_STEP2;
            }
            break;
        case STATE_SPECIAL_STEP2:
            if (address == 0x5555 && data == 0xaa) {
                flash->state = STATE_SPECIAL_STEP3;
            }
            break;
        case STATE_SPECIAL_STEP3:
            if (address == 0x2aaa && data == 0x55) {
                flash->state = STATE_PERFORM_ERASE;
            }
            break;

        case STATE_PERFORM_WRITE:
            /* The NOR flash accepts writing a byte! Only bits that are 1 can be set to 0.
             * Write the value right now to simplify the logic after */
            hw_log(flash->parent.host, false, 0, "[FLASH] Writing byte 0x%x (& %x = %x) @ 0x%x\n", data, flash->data[address], data & flash->data[address], address);
            flash->data[address] &= data;
            flash->dirty = 1;
            /* The byte being written must have bit 7 flipped, DQ6 must be toggled at each read */
            flash->writing_byte = data ^ 0x80;
            /* Writing a byte takes 20us on real hardware */
            vtimer_schedule_us(&flash->timer, 20);
            flash->state = STATE_PERFORM_WRITE_DELAY;
            break;

        case STATE_PERFORM_ERASE:
            if (data == 0x30) {
                /* Erase sector transaction! */
                /* Erasing a sector takes 25ms on real hardware */
                vtimer_schedule_us(&flash->timer, 25000);
                flash->state = STATE_PERFORM_ERASE_DELAY;
                /* Get the corresponding 4KB-sector to erase out of the 22-bit address */
                const uint32_t sector = address & 0x3ff000;
                hw_log(flash->parent.host, false, 0, "[FLASH] Erasing sector %d @ address 0x%x\n", sector / 4096, sector);
                flash->dirty = 1;
                memset(&flash->data[sector], 0xff, 4096);
            } else if (data == 0x10 && address == 0x5555) {
                /* Chip erase! */
                hw_log(flash->parent.host, false, 0, "[FLASH] Erasing chip\n");
                /* Erasing the chip takes 100ms on real hardware */
                vtimer_schedule_us(&flash->timer, 100000);
                flash->state = STATE_PERFORM_ERASE_DELAY;
                flash->dirty = 1;
                memset(flash->data, 0xff, flash->size);
            } else {
                /* Invalid state, try again */
                flash->state = STATE_IDLE;
                flash_write(device, address, data);
            }
            break;

        case STATE_PERFORM_ERASE_DELAY:
        case STATE_PERFORM_WRITE_DELAY:
            /* Delaying the write to simulate a real NOR flash */
            break;

        case STATE_SOFTWARE_ID:
            if (data == 0xf0) {
                flash->state = STATE_IDLE;
            }
            break;

        default:
            /* The combination was invalid, reset the state and retry, else,
             * the current byte being written would be lost and not part of the FSM. */
            flash->state = STATE_IDLE;
            flash_write(device, address, data);
            break;
    }
}


static void flash_idle_cb(void* userdata)
{
    flash_t* flash = (flash_t*) userdata;
    flash->state = STATE_IDLE;
}


int flash_init(flash_t* flash, vtimer_t* clock, const hw_host_t* host)
{
    if (flash == NULL) {
        return 1;
    }
    /* Empty flash contains FF bytes*/
    memset(flash, 0xFF, sizeof(*flash));
    flash->parent.host = host;
    flash->size = NOR_FLASH_SIZE_KB;
    flash->state = STATE_IDLE;
    flash->dirty = 0;
    vtimer_init_node(&flash->timer, clock, flash_idle_cb, flash);

#if CONFIG_NOR_FLASH_DYNAMIC_ARRAY
    flash->data = malloc(flash->size);
    if (flash->data == NULL) {
        hw_log(flash->parent.host, true, 0, "[FLASH] ERROR: could not allocate enough memory for the NOR flash\n");
        return 2;
    }
#endif

    device_init_mem_debug(DEVICE(flash), "nor_flash_dev", flash_read, flash_write, flash_debug_read, flash->size);
    return 0;
}

void flash_loaded(flash_t* flash)
{
    flash->state = STATE_IDLE;
    flash->dirty = 0;
}
