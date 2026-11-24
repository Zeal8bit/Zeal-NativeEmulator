/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <string.h>
#include "hw/hostfs.h"
#include "hw/host.h"
#define OPERATION_REG 15
#define OP_LAST 9
#define ZOS_PENDING 255
#define ZOS_FAILURE 1

static void set_status(zeal_hostfs_t* host, uint8_t status)
{
    host->registers[OPERATION_REG] = status;
}

/**
 * Define the I/O device operations
 */
static uint8_t io_read(device_t* dev, uint32_t addr)
{
    zeal_hostfs_t* hostfs = (zeal_hostfs_t*) dev;
    return hostfs->registers[addr & 0xf];
}


static void io_write(device_t* dev, uint32_t addr, uint8_t value)
{
    zeal_hostfs_t* hostfs = (zeal_hostfs_t*) dev;
    /**
     * I/O mapping for write is as follows:
     *  0x00 - Start operation
     *      1 - Open directory
     *      2 - Read directory
     *      3 - Open file
     *      4 - Read file
     *      5 - Close
     *  0x01 - Path address, LSB
     *  0x02 - Path address, MSB
     */
    if (addr == OPERATION_REG) {
        if (value <= OP_LAST) {
            if (hostfs->guard_pending && hostfs->registers[OPERATION_REG] == ZOS_PENDING) {
                hw_log_error(hostfs->parent.host, "[HostFS] Operation already pending\n");
                return;
            }
            set_status(hostfs, ZOS_PENDING);
            if (hostfs->dispatch) {
                hostfs->dispatch(hostfs->userdata, value);
            } else {
                set_status(hostfs, ZOS_FAILURE);
            }
        } else {
            hw_log_error(hostfs->parent.host, "[HostFS] Invalid operation %x\n", value);
            set_status(hostfs, ZOS_FAILURE);
        }
    } else if (addr <= 7) {
        hostfs->registers[addr] = value;
    } else {
        hw_log_error(hostfs->parent.host, "[HostFS] Unknown register 0x%x write\n", addr);
    }
}


int hostfs_init(zeal_hostfs_t* hostfs, mmu_t* mmu)
{
    memset(hostfs, 0, sizeof(zeal_hostfs_t));
    hostfs->mmu = mmu;
    device_init_io(DEVICE(hostfs), "hostfs_dev", io_read, io_write, 0x10);
    return 0;
}
