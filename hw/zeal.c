#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "hw/zeal.h"

#define CHECK_ERR(err)  \
    do {                \
        if (err)        \
            return err; \
    } while (0)

/**
 * @brief Callback invoked when the CPU tries to read a byte in memory space
 */
static uint8_t zeal_mem_read(void* opaque, uint16_t virt_addr)
{
    const zeal_t* machine    = (zeal_t*) opaque;
    const int phys_addr      = mmu_get_phys_addr(&machine->mmu, virt_addr);
    const map_entry_t* entry = &machine->mem_mapping[phys_addr / MMU_PAGE_SIZE];
    device_t* device         = entry->dev;
    const int start_addr     = entry->page_from * MMU_PAGE_SIZE;

    if (device) {
        return device->mem_region.read(device, phys_addr - start_addr);
    }

    printf("[INFO] No device replied to memory read: 0x%04x\n", phys_addr);
    return 0;
}

static void zeal_mem_write(void* opaque, uint16_t virt_addr, uint8_t data)
{
    const zeal_t* machine    = (zeal_t*) opaque;
    const int phys_addr      = mmu_get_phys_addr(&machine->mmu, virt_addr);
    const map_entry_t* entry = &machine->mem_mapping[phys_addr / MMU_PAGE_SIZE];
    device_t* device         = entry->dev;
    const int start_addr     = entry->page_from * MMU_PAGE_SIZE;

    if (device) {
        device->mem_region.write(device, phys_addr - start_addr, data);
    } else {
        printf("[INFO] No device replied to memory write: 0x%04x\n", phys_addr);
    }
}

static uint8_t zeal_io_read(void* opaque, uint16_t addr)
{
    zeal_t* machine          = (zeal_t*) opaque;
    const int low            = addr & 0xff;
    const map_entry_t* entry = &machine->io_mapping[low];
    device_t* device         = entry->dev;

    if (device) {
        device->io_region.upper_addr = addr >> 8;
        return device->io_region.read(device, low - entry->page_from);
    }

    printf("[INFO] No device replied to I/O read: 0x%04x\n", low);
    return 0;
}

static void zeal_io_write(void* opaque, uint16_t addr, uint8_t data)
{
    zeal_t* machine          = (zeal_t*) opaque;
    const int low            = addr & 0xff;
    const map_entry_t* entry = &machine->io_mapping[low];
    device_t* device         = entry->dev;

    if (device) {
        device->io_region.write(device, low - entry->page_from, data);
    } else {
        printf("[INFO] No device replied to I/O write: 0x%04x\n", low);
    }
}

/**
 * @brief Initialize the CPU and set the callbacks for the memory and I/O buses access.
 */
static void zeal_init_cpu(zeal_t* machine)
{
    z80_init(&machine->cpu);
    machine->cpu.userdata   = (void*) machine;
    machine->cpu.read_byte  = zeal_mem_read;
    machine->cpu.write_byte = zeal_mem_write;
    machine->cpu.port_in    = zeal_io_read;
    machine->cpu.port_out   = zeal_io_write;
}


static void zeal_add_io_device(zeal_t* machine, int region_start, device_t* dev)
{
    /* Start and end address of the region mapped for the device */
    const int region_size = dev->io_region.size;
    const int region_end  = region_start + region_size - 1;
    if (region_start >= IO_MAPPING_SIZE || region_end >= IO_MAPPING_SIZE || region_size == 0) {
        printf("%s: cannot register device, invalid region 0x%02x (%d bytes)\n", __func__, region_start, region_size);
        return;
    }

    /* Register the device in the array */
    for (int i = region_start; i <= region_end; i++) {
        machine->io_mapping[i] = (map_entry_t) {.dev = dev, .page_from = region_start};
    }
}

static void zeal_add_mem_device(zeal_t* machine, const int region_start, device_t* dev)
{
    const int region_size = dev->mem_region.size;
    const int region_end  = region_start + region_size - 1;
    if (region_start >= MEM_SPACE_SIZE || region_end >= MEM_SPACE_SIZE || region_size == 0) {
        printf("%s: cannot register device, invalid region 0x%02x (%d bytes)\n", __func__, region_start, region_size);
        return;
    }

    /* Make sure the alignment is correct too! */
    if ((region_start & (MEM_SPACE_ALIGN - 1)) != 0 || (region_size & (MEM_SPACE_ALIGN - 1)) != 0) {
        printf("%s: cannot register device, invalid alignment for region 0x%02x (%d bytes)\n", __func__, region_start,
               region_size);
        return;
    }

    /* Number of pages the device needs */
    const int start_page = region_start / MEM_SPACE_ALIGN;
    const int page_count = region_size / MEM_SPACE_ALIGN;

    for (int i = 0; i < page_count; i++) {
        const int page = start_page + i;
        map_entry_t* entry = &machine->mem_mapping[page];

        if (entry->dev != NULL) {
            printf("%s: cannot register device %s in page %d, device %s is already mapped\n",
                __func__, dev->name, page, entry->dev->name);
        }
        *entry = (map_entry_t) {.dev = dev, .page_from = start_page};
    }
}


int zeal_init(zeal_t* machine)
{
    int err = 0;
    if (machine == NULL) {
        return 1;
    }

    memset(machine, 0, sizeof(*machine));

    zeal_init_cpu(machine);

    // const mmu = new MMU();
    err = mmu_init(&machine->mmu);
    CHECK_ERR(err);

    // const rom = new ROM(this);
    err = flash_init(&machine->rom);
    CHECK_ERR(err);

    // const ram = new RAM(512*KB);
    err = ram_init(&machine->ram);
    CHECK_ERR(err);

    // const pio = new PIO(this);
    err = pio_init(machine, &machine->pio);
    CHECK_ERR(err);

    // const vchip = new VideoChip(this, pio, scale);
    err = zvb_init(&machine->zvb);
    CHECK_ERR(err);

    // const pio = new PIO(this);
    // const uart = new UART(this, pio);
    // const uart_web_serial = new UART_WebSerial(this, pio);
    // const i2c = new I2C(this, pio);
    // const keyboard = new Keyboard(this, pio);
    // // const keyboard = new Keyboard(this);
    // const ds1307 = new I2C_DS1307(this, i2c);
    // /* Extensions */
    // const compactflash = new CompactFlash(this);
    // /* We could pass an initial content to the EEPROM, but set it to null for the moment */
    // const eeprom = new I2C_EEPROM(this, i2c, null);

    // /* Create a HostFS to ease the file and directory access for the VM */
    // const hostfs = new HostFS(this.mem_read, this.mem_write);
    // const virtdisk = new VirtDisk(512, this.mem_read, this.mem_write);

    /* Register the devices in the memory space */
    zeal_add_mem_device(machine, 0x000000, &machine->rom.parent);
    if (machine->rom.size < NOR_FLASH_SIZE_KB_MAX) {
        /* Create a mirror in the upper 256KB */
        zeal_add_mem_device(machine, 0x040000, &machine->rom.parent);
    }

    zeal_add_mem_device(machine, 0x080000, &machine->ram.parent);
    zeal_add_mem_device(machine, 0x100000, &machine->zvb.parent);

    /* Register the devices in the I/O space */
    zeal_add_io_device(machine, 0xf0, &machine->mmu.parent);
    zeal_add_io_device(machine, 0x80, &machine->zvb.parent);
    zeal_add_io_device(machine, 0xd0, &machine->pio.parent);

    return 0;
}


int zeal_run(zeal_t* machine)
{    

    while (zvb_window_opened(&machine->zvb)) {
        const int elapsed_tstates = z80_step(&machine->cpu);
        /* TODO: Go through all the devices that have a tick function */
        zvb_tick(&machine->zvb, elapsed_tstates);
#if DEBUG
        z80_debug_output(&machine->cpu);
#endif
    }

    zvb_deinit(&machine->zvb);

    return 0;
}

