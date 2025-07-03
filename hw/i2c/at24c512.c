#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/log.h"
#include "hw/i2c/at24c512.h"

// #define debug log_printf
#define debug(...)

static uint8_t at24c512_read(i2c_device_t* dev) {
    at24c512_t* eeprom = (at24c512_t*)dev;
    const uint8_t byte = eeprom->data[eeprom->address];
    eeprom->address = (eeprom->address + 1) % AT24C512_SIZE;
    return byte;
}


static void at24c512_write(i2c_device_t* dev, uint8_t byte) {
    at24c512_t* eeprom = (at24c512_t*)dev;
    if (eeprom->count == 0) {
        eeprom->address = (byte << 8);  // High byte of address
    } else if (eeprom->count == 1) {
        eeprom->address |= byte;        // Low byte of address
    } else {
        if (!eeprom->writing) {
            /* Save the starting sector */
            eeprom->sector_written = eeprom->address / AT24C512_PAGE;
            eeprom->writing = true;
            debug("[EEPROM] Starting page write at address 0x%04X (sector 0x%04x)\n",
                  eeprom->address, eeprom->sector_written);
        }
        eeprom->data[eeprom->address] = byte;
        eeprom->address = (eeprom->address + 1) % AT24C512_SIZE;
        if ((eeprom->address % AT24C512_PAGE) == 0) {
            /* Page boundary reached, make the address roll back to the beginning of the sector */
            eeprom->address = eeprom->address - AT24C512_PAGE;
        }
    }
    eeprom->count++;
}


static void at24c512_start(i2c_device_t* dev) {
    at24c512_t* eeprom = (at24c512_t*)dev;
    eeprom->count = 0;
    eeprom->writing = false;
}


static void at24c512_stop(i2c_device_t* dev) {
    at24c512_t* eeprom = (at24c512_t*)dev;
    if (eeprom->writing && eeprom->file) {
        const int page_start = eeprom->sector_written * AT24C512_PAGE;
        fseek(eeprom->file, page_start, SEEK_SET);
        debug("[EEPROM] Writing back sector 0x%x to file\n", eeprom->sector_written);
        fwrite(&eeprom->data[page_start], 1, AT24C512_PAGE, eeprom->file);
    }
    eeprom->writing = false;
}

int at24c512_init(at24c512_t* eeprom, const char* filename)
{
    if (eeprom == NULL) {
        return 1;
    }

    eeprom->parent.address = AT24C512_ADDR;
    eeprom->parent.start = at24c512_start;
    eeprom->parent.read = at24c512_read;
    eeprom->parent.write = at24c512_write;
    eeprom->parent.stop = at24c512_stop;
    eeprom->count = 0;
    eeprom->writing = false;
    eeprom->file = NULL;
    memset(eeprom->data, 0, AT24C512_SIZE);

    if (filename) {
        FILE* file = fopen(filename, "rb+");
        if (file) {
            size_t count = fread(eeprom->data, 1, AT24C512_SIZE, file);
            log_printf("[EEPROM] Loaded from %s\n", filename);
            if (count < AT24C512_SIZE) {
                log_err_printf("[EEPROM] Warning: image size is smaller than EEPROM size\n");
            }
            eeprom->file = file;
        } else {
            log_err_printf("[EEPROM] Could not open image %s\n", filename);
        }
    }

    return 0;
}


void at24c512_deinit(at24c512_t* eeprom)
{
    if (eeprom == NULL || eeprom->file == NULL) {
        return;
    }
    fflush(eeprom->file);
    fclose(eeprom->file);
}
