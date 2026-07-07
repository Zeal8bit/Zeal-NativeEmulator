/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include "hw/device.h"

#define MMU_PAGE_SIZE   (16 * 1024)
#define MMU_PAGES_COUNT 4


/* Size of the memory space */
#define MEM_SPACE_SIZE (4 * 1024 * 1024)
/* Granularity of the memory space (smallest page a device can be allocated on) */
#define MEM_SPACE_ALIGN (16 * 1024)
/* Size of the memory mapping that will contain our devices */
#define MEM_MAPPING_SIZE ((MEM_SPACE_SIZE) / (MEM_SPACE_ALIGN))

/**
 * @brief Size of the I/O space, considering that it has a granularity of a single byte
 */
#define IO_MAPPING_SIZE 256

/**
 * @brief Each device needs to be associated to the physical page where its mapping starts.
 * Since we have at most 256 pages, we can use a single byte for that
 */
typedef struct {
    device_t* dev;
    int page_from;
} map_entry_t;

/**
 * @brief Cached device for a virtual page, resolved when MMU page register changes.
 * Avoids runtime mem_mapping lookup on every memory access.
 */
typedef struct {
    device_t* dev;
    uint32_t base_phys; // Physical address where device region starts (page_from * MEM_SPACE_ALIGN)
} mmu_cached_page_t;


typedef struct {
    device_t parent;

    /* Device registration tables (set once during init) */
    map_entry_t io_mapping[IO_MAPPING_SIZE];
    map_entry_t mem_mapping[MEM_MAPPING_SIZE];

    struct {
        uint8_t (*read_byte)(void*, uint16_t);
        void (*write_byte)(void*, uint16_t, uint8_t);
    } ops;

    /* MMU page registers */
    uint8_t pages[MMU_PAGES_COUNT];

    /* Cached device per virtual page, resolved on page register write */
    mmu_cached_page_t vpages[MMU_PAGES_COUNT];
} mmu_t;


int mmu_init(mmu_t* mmu);

/**
 * @brief Get the physical address out of a virtual address
 */
int mmu_get_phys_addr(const mmu_t* mmu, uint16_t virt_addr);


/**
 * @brief Read a byte from a virtual address via cached vpages.
 * Returns 0 if no device is mapped at that address.
 */
static inline uint8_t mmu_virt_read_byte(const mmu_t* mmu, uint16_t virt_addr)
{
    const int idx = (virt_addr >> 14) & 0x3;
    const mmu_cached_page_t* vp = &mmu->vpages[idx];
    if (!vp->dev) return 0;
    const uint32_t phys = ((uint32_t)mmu->pages[idx] << 14) | (virt_addr & (MMU_PAGE_SIZE - 1));
    return vp->dev->mem_region.read(vp->dev, phys - vp->base_phys);
}


/**
 * @brief Write a byte to a virtual address via cached vpages.
 */
static inline void mmu_virt_write_byte(mmu_t* mmu, uint16_t virt_addr, uint8_t data)
{
    const int idx = (virt_addr >> 14) & 0x3;
    const mmu_cached_page_t* vp = &mmu->vpages[idx];
    if (!vp->dev) return;
    const uint32_t phys = ((uint32_t)mmu->pages[idx] << 14) | (virt_addr & (MMU_PAGE_SIZE - 1));
    vp->dev->mem_region.write(vp->dev, phys - vp->base_phys, data);
}


/**
 * @brief Read a byte from a physical address via the device mapping table.
 * Returns 0 if no device is mapped at that address.
 */
static inline uint8_t mmu_phys_read_byte(const mmu_t* mmu, uint32_t phys_addr)
{
    if (phys_addr >= MEM_SPACE_SIZE) return 0;
    const int page = phys_addr / MEM_SPACE_ALIGN;
    const map_entry_t* entry = &mmu->mem_mapping[page];
    if (!entry->dev) return 0;
    return entry->dev->mem_region.read(entry->dev, phys_addr - (uint32_t)entry->page_from * MEM_SPACE_ALIGN);
}


/**
 * @brief Write a byte to a physical address via the device mapping table.
 */
static inline void mmu_phys_write_byte(mmu_t* mmu, uint32_t phys_addr, uint8_t data)
{
    if (phys_addr >= MEM_SPACE_SIZE) return;
    const int page = phys_addr / MEM_SPACE_ALIGN;
    const map_entry_t* entry = &mmu->mem_mapping[page];
    if (!entry->dev) return;
    entry->dev->mem_region.write(entry->dev, phys_addr - (uint32_t)entry->page_from * MEM_SPACE_ALIGN, data);
}


/**
 * @brief Read a byte from an I/O port. Uses the flat 256-byte I/O mapping.
 */
static inline uint8_t mmu_io_read_byte(mmu_t* mmu, uint16_t addr)
{
    const int low = addr & 0xff;
    const map_entry_t* entry = &mmu->io_mapping[low];
    if (!entry->dev || !entry->dev->io_region.read) return 0;
    entry->dev->io_region.upper_addr = addr >> 8;
    return entry->dev->io_region.read(entry->dev, low - entry->page_from);
}


/**
 * @brief Write a byte to an I/O port. Uses the flat 256-byte I/O mapping.
 */
static inline void mmu_io_write_byte(mmu_t* mmu, uint16_t addr, uint8_t data)
{
    const int low = addr & 0xff;
    const map_entry_t* entry = &mmu->io_mapping[low];
    if (!entry->dev || !entry->dev->io_region.write) return;
    entry->dev->io_region.upper_addr = addr >> 8;
    entry->dev->io_region.write(entry->dev, low - entry->page_from, data);
}


/**
 * @brief Register an I/O device in the MMU I/O mapping table
 */
void mmu_register_io_device(mmu_t* mmu, int region_start, device_t* dev);

/**
 * @brief Register a memory device in the MMU memory mapping table
 */
void mmu_register_mem_device(mmu_t* mmu, int region_start, device_t* dev);


/**
 * @brief Read len bytes from consecutive virtual addresses into buf.
 */
void mmu_virt_read_array(const mmu_t* mmu, uint16_t virt_addr, uint8_t* buf, uint16_t len);

/**
 * @brief Write len bytes from buf to consecutive virtual addresses.
 */
void mmu_virt_write_array(mmu_t* mmu, uint16_t virt_addr, const uint8_t* buf, uint16_t len);

/**
 * @brief Read len bytes from consecutive physical addresses into buf.
 */
void mmu_phys_read_array(const mmu_t* mmu, uint32_t phys_addr, uint8_t* buf, uint16_t len);

/**
 * @brief Write len bytes from buf to consecutive physical addresses.
 */
void mmu_phys_write_array(mmu_t* mmu, uint32_t phys_addr, const uint8_t* buf, uint16_t len);


