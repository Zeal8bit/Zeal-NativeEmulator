#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/ram.h"
#include "utils/log.h"


static uint8_t ram_read(device_t* dev, uint32_t addr) {
    ram_t* r = (ram_t*)dev;
    if(addr >= r->size) {
        log_err_printf("[RAM] Invalid read size: %08x\n", addr);
        return 0;
    }
    return r->data[addr];
}


static void ram_write(device_t* dev, uint32_t addr, uint8_t data) {
    ram_t* r = (ram_t*)dev;
    if(addr >= r->size) {
        log_err_printf("[RAM] Invalid read size: %08x\n", addr);
        return;
    }
    r->data[addr] = data;
}


int ram_init(ram_t *r) {
    if(r == NULL) {
        return 1;
    }

    memset(r, 0, sizeof(*r));
    r->size = RAM_SIZE_KB;

#if CONFIG_RAM_DYNAMIC_ARRAY
    r->data = malloc(r->size);
    if (r->data == NULL) {
        log_err_printf("[RAM] ERROR: could not allocate enough memory for the RAM\n");
        return 2;
    }
#endif

    device_init_mem(DEVICE(r), "ram_dev", ram_read, ram_write, r->size);
    return 0;
}
