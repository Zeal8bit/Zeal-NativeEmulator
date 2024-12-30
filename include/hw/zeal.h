#pragma once

#include <stdint.h>
#include "hw/z80.h"
#include "hw/device.h"
#include "hw/mmu.h"
#include "hw/flash.h"

typedef uint8_t dev_idx_t;

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
 * @brief Maximum number of devices that can be attached to the computer. This is purely
 * arbitrary and not a hardware limitation. The goal is to reduce the footprint in memory.
 */
#define ZEAL_MAX_DEVICE_COUNT 32


/**
 * @brief Each device needs to be associated to the physical page where its mapping starts.
 * Since we have at most 256 pages, we can use a single byte for that
 */
typedef struct {
    device_t* dev;
    int page_from;
} map_entry_t;

struct zeal_t {
    /* Memory regions related, the I/O space's granularity is a single byte */
    map_entry_t io_mapping[IO_MAPPING_SIZE];
    map_entry_t mem_mapping[MEM_MAPPING_SIZE];

    z80 cpu;
    mmu_t mmu;
    flash_t rom;
};

typedef struct zeal_t zeal_t;


int zeal_init(zeal_t* machine);
