/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include "hw/device.h"
#include "hw/mmu.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    device_t parent;
    uint8_t registers[16];
    mmu_t* mmu;
    /* Frontend backend; it may complete synchronously or leave the status
     * pending. Pending work survives reset, and the frontend detaches the
     * backend before teardown. */
    void* userdata;
    void (*dispatch)(void*, uint8_t operation);
    bool guard_pending;
} zeal_hostfs_t;

int hostfs_init(zeal_hostfs_t* hostfs, mmu_t* mmu);

#ifdef __cplusplus
} /* extern "C" */
#endif
