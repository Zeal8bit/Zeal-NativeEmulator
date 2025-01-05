#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/mmu.h"


static uint8_t mmu_read(device_t* dev, uint32_t addr)
{
    mmu_t* mmu    = (mmu_t*) dev;
    const int idx = (dev->io_region.upper_addr >> 6) & 3;

    return mmu->pages[idx];
}


static void mmu_write(device_t* dev, uint32_t addr, uint8_t data)
{
    mmu_t* mmu      = (mmu_t*) dev;
    const int idx   = addr & 0x3;
    mmu->pages[idx] = data;
}


int mmu_init(mmu_t* mmu)
{
    if (mmu == NULL) {
        return 1;
    }
    /* All the pages will be initialized to 0 */
    memset(mmu, 0, sizeof(*mmu));
    device_init_io(DEVICE(mmu), "mmu_dev", mmu_read, mmu_write, 0x10);
    return 0;
}


int mmu_get_phys_addr(const mmu_t* mmu, uint16_t virt_addr)
{
    if (mmu == NULL) {
        printf("FATAL ERROR: MMU must not be NULL!\n");
        return -1;
    }
    /* Get 22-bit address from 16-bit address */
    const int idx     = (virt_addr >> 14) & 0x3;
    const int highest = mmu->pages[idx];
    /* Highest bits are from the page, remaining 16KB are from address */
    return (highest << 14) | (virt_addr & (MMU_PAGE_SIZE - 1));
}
