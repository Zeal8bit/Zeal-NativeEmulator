/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/device.h"
#include "hw/mmu.h"

typedef struct {
    device_t parent;
    uint8_t registers[16];
    mmu_t* mmu;
    /* Borrowed backend may complete synchronously or leave status pending.
     * Pending work survives reset; the owner detaches it before teardown. */
    void* userdata;
    void (*dispatch)(void*, uint8_t operation);
    bool guard_pending;
} zeal_hostfs_t;

int hostfs_init(zeal_hostfs_t* hostfs, mmu_t* mmu);
