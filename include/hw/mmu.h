/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include "hw/device.h"

#define MMU_PAGE_SIZE (16 * 1024)

typedef struct {
    device_t parent;
    uint8_t pages[4];
} mmu_t;


int mmu_init(mmu_t* mmu);

/**
 * @brief Get the physical address out of a virtual address
 */
int mmu_get_phys_addr(const mmu_t* mmu, uint16_t virt_addr);
