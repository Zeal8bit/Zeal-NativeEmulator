#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/zeal.h"

#if CONFIG_ENABLE_GDB_SERVER
    #include "dbg/zeal_dbg.h"
#endif

#define MAKE16(a,b)     ((a) << 8 | (b))
#define MSB8(a)         (((a) >> 8) & 0xff)
#define LSB8(a)         (((a) >> 0) & 0xff)

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


int zeal_init(zeal_t* machine, bool debug_server)
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
    // const vchip = new VideoChip(this, pio, scale);
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

    /* Register the devices in the I/O space */
    zeal_add_io_device(machine, 0xf0, &machine->mmu.parent);


    if (debug_server) {
#if CONFIG_ENABLE_GDB_SERVER
        zeal_dbg_init(machine);
#else
        printf("%s: cannot start debug server, recompile with CONFIG_ENABLE_GDB_SERVER\n", __func__);
#endif
    }

    return 0;
}

int zeal_run(zeal_t* machine)
{
#if CONFIG_ENABLE_GDB_SERVER
    printf("Waiting for GDB client to connect on port " DEBUG_PORT "\n");
    bool success = gdbstub_init(&machine->dbg_serv, &machine->dbg_ops,
                                machine->dbg_arch, "127.0.0.1:" DEBUG_PORT);
    if (!success) {
        printf("%s: error initializing GDB server\n", __func__);
        return 1;
    }

    /* Try to start the server */
    if (!gdbstub_run(&machine->dbg_serv, machine)) {
        fprintf(stderr, "%s: fail to run in debug mode.\n", __func__);
        return 2;
    }
#else 
    int win_opened = 1;
    while (win_opened) {
//         for (int i = 0; i < 32; i++) {
//             z80_step(&machine->cpu);
// #if DEBUG
//             z80_debug_output(&machine->cpu);
// #endif
//         }
        win_opened = zvb_render(&machine->zvb);
    }
#endif

    return 0;
}


void zeal_close(zeal_t* machine)
{
    // TODO: close the submodules
    if (machine == NULL) {
        return;
    }
    gdbstub_close(&machine->dbg_serv);
}


#if CONFIG_ENABLE_GDB_SERVER
/**
 * DEBUG related
 */

static int zeal_dbg_read_reg(void *args, int regno, size_t *reg_value)
{
    zeal_t* machine = (zeal_t*) args;
    if (regno > Z80_REG_LAST) {
        return -1;
    }

    z80* cpu = &machine->cpu;

    switch(regno) {
        case Z80_AF_REG:
            *reg_value = MAKE16(cpu->a, z80_get_f(cpu));
            break;
        case Z80_BC_REG:
            *reg_value = MAKE16(cpu->b, cpu->c);
            break;
        case Z80_DE_REG:
            *reg_value = MAKE16(cpu->d, cpu->e);
            break;
        case Z80_HL_REG:
            *reg_value = MAKE16(cpu->h, cpu->l);
            break;
        case Z80_SP_REG:
            *reg_value = cpu->sp;
            break;
        case Z80_PC_REG:
            *reg_value = cpu->pc;
            break;
        case Z80_IX_REG:
            *reg_value = cpu->ix;
            break;
        case Z80_IY_REG:
            *reg_value = cpu->iy;
            break;
        case Z80_AFP_REG:
            *reg_value = MAKE16(cpu->a_, cpu->f_);
            break;
        case Z80_BCP_REG:
            *reg_value = MAKE16(cpu->b_, cpu->c_);
            break;
        case Z80_DEP_REG:
            *reg_value = MAKE16(cpu->d_, cpu->e_);
            break;
        case Z80_HLP_REG:
            *reg_value = MAKE16(cpu->h_, cpu->l_);
            break;
        case Z80_IR_REG:
            *reg_value = MAKE16(cpu->i, cpu->r);
            break;
    }

    return 0;
}


static int zeal_dbg_write_reg(void *args, int regno, size_t data)
{
    zeal_t* machine = (zeal_t*) args;
    if (regno > Z80_REG_LAST) {
        return -1;
    }

    z80* cpu = &machine->cpu;
    switch(regno) {
        case Z80_AF_REG:
            cpu->a = MSB8(data);
            z80_set_f(cpu, LSB8(data));
            break;
        case Z80_BC_REG:
            cpu->b = MSB8(data);
            cpu->c = LSB8(data);
            break;
        case Z80_DE_REG:
            cpu->d = MSB8(data);
            cpu->e = LSB8(data);
            break;
        case Z80_HL_REG:
            cpu->h = MSB8(data);
            cpu->l = LSB8(data);
            break;
        case Z80_SP_REG:
            cpu->sp = data & 0xffff;
            break;
        case Z80_PC_REG:
            cpu->pc = data & 0xffff;
            break;
        case Z80_IX_REG:
            cpu->ix = data & 0xffff;
            break;
        case Z80_IY_REG:
            cpu->iy = data & 0xffff;
            break;
        case Z80_AFP_REG:
            cpu->a_ = MSB8(data);
            cpu->f_ = LSB8(data);
            break;
        case Z80_BCP_REG:
            cpu->b_ = MSB8(data);
            cpu->c_ = LSB8(data);
            break;
        case Z80_DEP_REG:
            cpu->d_ = MSB8(data);
            cpu->e_ = LSB8(data);
            break;
        case Z80_HLP_REG:
            cpu->h_ = MSB8(data);
            cpu->l_ = LSB8(data);
            break;
        case Z80_IR_REG:
            cpu->i = MSB8(data);
            cpu->r = LSB8(data);
            break;
    }
    return 0;
}


static int zeal_dbg_read_mem(void *args, size_t addr, size_t len, void *val)
{
    printf("%s, 0x%lx, 0x%lx\n", __func__, addr, len);
    uint8_t* val_ptr = (uint8_t*) val;

    /* If upper bit in 32-bit address is set, interpret it as a physical address */
    if (addr & 0x80000000) {
        printf("TODO: PHYSICAL ADDRESS READ\n");
    } else if (addr <= 0xffff) {
        for (size_t i = 0; i < len; i++) {
            val_ptr[i] = zeal_mem_read(args, (uint16_t) (addr + i));
        }
    } else {
        // Invalid address
        return -1;
    }

    return 0;
}


static int zeal_dbg_write_mem(void *args, size_t addr, size_t len, void *val)
{
    printf("%s\n", __func__);
    uint8_t* val_ptr = (uint8_t*) val;

    /* If upper bit in 32-bit address is set, interpret it as a physical address */
    if (addr & 0x80000000) {
        printf("TODO: PHYSICAL ADDRESS READ\n");
    } else if (addr <= 0xffff) {
        for (size_t i = 0; i < len; i++) {
            zeal_mem_write(args, (uint16_t) (addr + i), val_ptr[i]);
        }
    } else {
        // Invalid address
        return -1;
    }

    return 0;
}


static bool zeal_dbg_pc_in_breakpoints(zeal_t* machine)
{
    const uint16_t addr = machine->cpu.pc;
    // FIXME: Naive implementation... Using a binary tree would be better
    for (int i = 0; i < ZEAL_MAX_BREAKPOINTS; i++) {
        if (machine->breakpoints[i].addr == addr) {
            return true;
        }
    }
    return false;
}


static gdb_action_t zeal_dbg_cont(void *args)
{
    printf("%s\n", __func__);
    zeal_t* machine = (zeal_t*) args;

    do {
        // FIXME: Use the (future) "generic" step function which takes into
        // account the t-states callbacks and the GUI timings.
        z80_step(&machine->cpu);
    } while (!machine->brk_exec && !zeal_dbg_pc_in_breakpoints(machine));

    printf("Stopped at 0x%x\n", machine->cpu.pc);

    return ACT_RESUME;
}


static gdb_action_t zeal_dbg_stepi(void *args)
{
    printf("%s\n", __func__);
    zeal_t* machine = (zeal_t*) args;
    z80_step(&machine->cpu);
    return ACT_RESUME;
}


static bool zeal_dbg_set_bp(void *args, size_t addr, bp_type_t type)
{
    printf("%s 0x%lx (%d)\n", __func__, addr, type);
    zeal_t* machine = (zeal_t*) args;

    if (addr > 0xffff) {
        return false;
    }

    // FIXME: Very naive implementation currently
    for (int i = 0; i < ZEAL_MAX_BREAKPOINTS; i++) {
        if (machine->breakpoints[i].addr == 0) {
            machine->breakpoints[i].addr = addr;
            return true;
        }
    }

    (void) type;
    return false;
}


static bool zeal_dbg_del_bp(void *args, size_t addr, bp_type_t type)
{
    printf("%s 0x%lx (%d)\n", __func__, addr, type);
    zeal_t* machine = (zeal_t*) args;

    if (addr > 0xffff) {
        return true;
    }

    // FIXME: Very naive implementation currently
    for (int i = 0; i < ZEAL_MAX_BREAKPOINTS; i++) {
        if (machine->breakpoints[i].addr == addr) {
            machine->breakpoints[i].addr = 0;
            return true;
        }
    }

    (void) type;
    return true;
}


static void zeal_dbg_on_interrupt(void *args)
{
    printf("%s\n", __func__);
    zeal_t* machine = (zeal_t*) args;
    machine->brk_exec = true;
}


static int zeal_dbg_init(void* arg)
{
    zeal_t *machine = (zeal_t *) arg;

    /* We need to provide the number of registers (16-bit pairs) and the size of each */
    machine->dbg_arch = (arch_info_t) {
        .reg_num     = ZEAL_Z80_NUM_REGS,
        .reg_byte    = ZEAL_Z80_REG_BYTES,
        .target_desc = "z80-full",
    };
    
    /* Populate all the callbacks used for debugging */
    machine->dbg_ops = (target_ops) {
        .read_reg = zeal_dbg_read_reg,
        .write_reg = zeal_dbg_write_reg,
        .read_mem = zeal_dbg_read_mem,
        .write_mem = zeal_dbg_write_mem,
        .cont = zeal_dbg_cont,
        .stepi = zeal_dbg_stepi,
        .set_bp = zeal_dbg_set_bp,
        .del_bp = zeal_dbg_del_bp,
        .on_interrupt = zeal_dbg_on_interrupt,
    };

    return 0;
}
#endif // CONFIG_ENABLE_GDB_SERVER
