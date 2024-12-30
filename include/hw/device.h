#pragma once

#define DEVICE(dev) &dev->parent

typedef struct device_t device_t;

/**
 * @brief Structure defining a region in memory
 */
typedef struct {
    uint8_t (*read)(device_t* dev, uint32_t addr);
    void (*write)(device_t* dev, uint32_t addr, uint8_t data);
    int size;
} region_t;

struct device_t {
    const char* name; // Used for debugging
    region_t io_region;
    region_t mem_region;
};


static inline void device_init_io(device_t* dev, const char* name, uint8_t (*read)(device_t*, uint32_t),
                                  void (*write)(device_t*, uint32_t, uint8_t), int size)
{
    dev->name            = name;
    dev->io_region.size  = size;
    dev->io_region.read  = read;
    dev->io_region.write = write;
}


static inline void device_init_mem(device_t* dev, const char* name, uint8_t (*read)(device_t*, uint32_t),
                                   void (*write)(device_t*, uint32_t, uint8_t), int size)
{
    dev->name             = name;
    dev->mem_region.size  = size;
    dev->mem_region.read  = read;
    dev->mem_region.write = write;
}
