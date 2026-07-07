/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/mmu.h"
#include "utils/log.h"


static uint8_t mmu_read(device_t* dev, uint32_t addr)
{
    (void)addr; // unused
    mmu_t* mmu    = (mmu_t*) dev;
    const int idx = (dev->io_region.upper_addr >> 6) & 3;
    (void) addr;

    return mmu->pages[idx];
}


static void mmu_resolve_vpage(mmu_t* mmu, int idx)
{
    const int phys_page = mmu->pages[idx];
    mmu->vpages[idx].dev       = mmu->mem_mapping[phys_page].dev;
    mmu->vpages[idx].base_phys = mmu->mem_mapping[phys_page].page_from * (uint32_t)MEM_SPACE_ALIGN;
}


static void mmu_write(device_t* dev, uint32_t addr, uint8_t data)
{
    mmu_t* mmu      = (mmu_t*) dev;
    const int idx   = addr & 0x3;
    mmu->pages[idx] = data;
    mmu_resolve_vpage(mmu, idx);
}


static void mmu_reset(device_t* dev)
{
    mmu_t* mmu = (mmu_t*) dev;
    /* On the real hardware, MMU reset only sets page 0 */
    mmu->pages[0] = 0;
    mmu_resolve_vpage(mmu, 0);
}


int mmu_init(mmu_t* mmu)
{
    if (mmu == NULL) {
        return 1;
    }
    /* All the pages will be initialized to 0 */
    memset(mmu, 0, sizeof(*mmu));
    memset(mmu->vpages, 0, sizeof(mmu->vpages));
    device_init_io(DEVICE(mmu), "mmu_dev", mmu_read, mmu_write, 0x10);
    device_register_reset(DEVICE(mmu), mmu_reset);
    return 0;
}


void mmu_register_io_device(mmu_t* mmu, int region_start, device_t* dev)
{
    const int region_size = dev->io_region.size;
    const int region_end  = region_start + region_size - 1;
    if (region_start >= IO_MAPPING_SIZE || region_end >= IO_MAPPING_SIZE || region_size == 0) {
        log_err_printf("%s: cannot register device, invalid region 0x%02x (%d bytes)\n", __func__, region_start, region_size);
        return;
    }
    for (int i = region_start; i <= region_end; i++) {
        mmu->io_mapping[i] = (map_entry_t) {.dev = dev, .page_from = region_start};
    }
}


void mmu_register_mem_device(mmu_t* mmu, int region_start, device_t* dev)
{
    const int region_size = dev->mem_region.size;
    const int region_end  = region_start + region_size - 1;
    if (region_start >= MEM_SPACE_SIZE || region_end >= MEM_SPACE_SIZE || region_size == 0) {
        log_err_printf("%s: cannot register device, invalid region 0x%02x (%d bytes)\n", __func__, region_start, region_size);
        return;
    }

    /* Make sure the alignment is correct too! */
    if ((region_start & (MEM_SPACE_ALIGN - 1)) != 0 || (region_size & (MEM_SPACE_ALIGN - 1)) != 0) {
        log_err_printf("%s: cannot register device, invalid alignment for region 0x%02x (%d bytes)\n", __func__, region_start,
               region_size);
        return;
    }

    const int start_page = region_start / MEM_SPACE_ALIGN;
    const int page_count = region_size / MEM_SPACE_ALIGN;

    for (int i = 0; i < page_count; i++) {
        const int page = start_page + i;
        map_entry_t* entry = &mmu->mem_mapping[page];

        if (entry->dev != NULL) {
            log_err_printf("%s: cannot register device %s in page %d, device %s is already mapped\n",
                __func__, dev->name, page, entry->dev->name);
        }
        *entry = (map_entry_t) {.dev = dev, .page_from = start_page};
    }

    /* Re-resolve vpages that map to the physical pages we just registered */
    for (int i = 0; i < MMU_PAGES_COUNT; i++) {
        const int phys_page = mmu->pages[i];
        if (phys_page >= start_page && phys_page < start_page + page_count) {
            mmu_resolve_vpage(mmu, i);
        }
    }
}


int mmu_get_phys_addr(const mmu_t* mmu, uint16_t virt_addr)
{
    if (mmu == NULL) {
        log_err_printf("[MMU] ERROR: MMU must not be NULL!\n");
        return -1;
    }
    /* Get 22-bit address from 16-bit address */
    const int idx     = (virt_addr >> 14) & 0x3;
    const int highest = mmu->pages[idx];
    /* Highest bits are from the page, remaining 16KB are from address */
    return (highest << 14) | (virt_addr & (MMU_PAGE_SIZE - 1));
}
