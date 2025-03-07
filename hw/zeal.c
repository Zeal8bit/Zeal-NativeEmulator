#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/zeal.h"
#include "utils/config.h"

int zeal_debugger_init(zeal_t* machine, dbg_t* dbg);

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

    printf("[INFO] No device replied to memory read: 0x%04x (PC @ 0x%04x)\n", phys_addr, machine->cpu.pc);
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


static void zeal_read_keyboard(zeal_t* machine, int delta) {
    static uint8_t RAYLIB_KEYS[384];
    int keyCode;
    // look for newly pressed keys
    while((keyCode = GetKeyPressed())) {
        RAYLIB_KEYS[keyCode] = 1;
        key_pressed(&machine->keyboard, keyCode);
    }

    // look for newly released keys
    for(uint16_t i = 0; i < sizeof(RAYLIB_KEYS); i++) {
        if(RAYLIB_KEYS[i] && IsKeyReleased(i)) {
            RAYLIB_KEYS[i] = 0;
            key_released(&machine->keyboard, i);
        }
    }

    keyboard_send_next(&machine->keyboard, &machine->pio, delta);
}


int zeal_debug_enable(zeal_t* machine)
{
    int ret = 0;
    machine->dbg_enabled = true;
    machine->dbg_state = ST_PAUSED;
    if(config.debugger.enabled <= DEBUGGER_STATE_DISABLED) SetWindowSize(WIN_VISIBLE_WIDTH, WIN_VISIBLE_HEIGHT);
    if(machine->dbg_ui == NULL) {
        ret = debugger_ui_init(&machine->dbg_ui, &machine->zvb_out);
    }
    return ret;
}


int zeal_debug_disable(zeal_t* machine)
{
    machine->dbg_enabled = false;
    machine->dbg_state = ST_RUNNING;
    SetWindowSize(config.window.width, config.window.height);
    return 0;
}


static void zeal_debug_toggle(zeal_t* machine)
{
    if (machine->dbg_enabled) {
        zeal_debug_disable(machine);
    } else {
        zeal_debug_enable(machine);
    }
}


int zeal_init(zeal_t* machine, config_t* config)
{
    int err = 0;
    if (machine == NULL) {
        return 1;
    }

    memset(machine, 0, sizeof(*machine));
    /* Set the debug mode in the machine structure as soon as possible */
    if (config != NULL) {
        machine->dbg_enabled = config->debugger.enabled > DEBUGGER_STATE_DISABLED;
    }

    /* Initialize the UI. It must be done before any shader is created! */
    SetTraceLogLevel(WIN_LOG_LEVEL);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    /* Not in debug mode, create the window as big as the emulated screen */
    InitWindow(config->window.width, config->window.height, WIN_NAME);
    config_window_set();

#if !BENCHMARK
    SetTargetFPS(60);
#endif

    /* Since we want to enable scaling, make the ZVB output always go to a texture first */
    machine->zvb_out = LoadRenderTexture(ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT);

    /* initialize the debugger */
    zeal_debugger_init(machine, &machine->dbg);
    /* Load symbols if provided */
    if (config && config->arguments.map_file) {
        debugger_load_symbols(&machine->dbg, config->arguments.map_file);
    }
    if (machine->dbg_enabled) {
        zeal_debug_enable(machine);
        /* Force the machine in RUNNING mode */
        machine->dbg_state = ST_RUNNING;
    }

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

    /* Flip the rendering if we are not in debug mode (since we will draw directly to the screen) */
    err = zvb_init(&machine->zvb, false);
    CHECK_ERR(err);

    // const uart = new UART(this, pio);
    err = uart_init(&machine->uart, &machine->pio);
    CHECK_ERR(err);
    // const uart_web_serial = new UART_WebSerial(this, pio);

    // const i2c = new I2C(this, pio);

    // const keyboard = new Keyboard(this, pio);
    err = keyboard_init(&machine->keyboard, &machine->pio);
    CHECK_ERR(err);

    // const ds1307 = new I2C_DS1307(this, i2c);

    // /* Extensions */
    // const compactflash = new CompactFlash(this);

    // /* We could pass an initial content to the EEPROM, but set it to null for the moment */
    // const eeprom = new I2C_EEPROM(this, i2c, null);

    // /* Create a HostFS to ease the file and directory access for the VM */
    // const hostfs = new HostFS(this.mem_read, this.mem_write);
    const memory_op_t ops = {
        .read_byte = zeal_mem_read,
        .write_byte = zeal_mem_write,
        .opaque = machine
    };
    err = hostfs_init(&machine->hostfs, &ops);
    CHECK_ERR(err);

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
    zeal_add_io_device(machine, 0x80, &machine->zvb.parent);
    zeal_add_io_device(machine, 0xc0, &machine->hostfs.parent);
    zeal_add_io_device(machine, 0xd0, &machine->pio.parent);
    zeal_add_io_device(machine, 0xe0, &machine->keyboard.parent);
    zeal_add_io_device(machine, 0xf0, &machine->mmu.parent);

    return 0;
}


static void zeal_dbg_mode_display(zeal_t* machine)
{
    /**
     * Prepare the rendering, if the returned value is true, we can
     * proceed to rendering, else, we don't need to update the view.
     * However, if the CPU is paused (breakpoint/step), force the rendering.
     */
    if (zvb_prepare_render(&machine->zvb)) {
        /* Display all the devices that have a render function */
        BeginTextureMode(machine->zvb_out);
            zvb_render(&machine->zvb);
        EndTextureMode();
    } else if (machine->dbg_state == ST_PAUSED) {
        /* No need for `prepare` in this case */
        BeginTextureMode(machine->zvb_out);
            zvb_force_render(&machine->zvb);
        EndTextureMode();
    } else {
        /* Do not proceed, the CPU is currently running and the ZVB doens't need to be refreshed yet */
        return;
    }

    debugger_ui_prepare_render(machine->dbg_ui, &machine->dbg);
    BeginDrawing();
        /* Grey brackground */
        ClearBackground((Color){ 0x63, 0x63, 0x63, 0xff });
        debugger_ui_render(machine->dbg_ui, &machine->dbg);
    EndDrawing();
}


/**
 * @brief Run Zeal 8-bit Computer VM in debug mode
 */
static int zeal_dbg_mode_run(zeal_t* machine)
{
    if (machine->dbg_state != ST_PAUSED) {
        const int elapsed_tstates = z80_step(&machine->cpu);
        /* Go through all the devices that have a tick function */
        zvb_tick(&machine->zvb, elapsed_tstates);
        /* Send keyboard keys to Zeal MV only if its window is focused */
        if (debugger_ui_main_view_focused(machine->dbg_ui)) {
            zeal_read_keyboard(machine, elapsed_tstates);
        }

        /* Check if we reached a breakpoint or if we have to do a single step */
        if (machine->dbg_state == ST_REQ_STEP ||
            debugger_is_breakpoint_set(&machine->dbg, machine->cpu.pc))
        {
            machine->dbg_state = ST_PAUSED;
        }
    }

    zeal_dbg_mode_display(machine);
    return 0;
}


/**
 * @brief Run Zeal 8-bit Computer VM in normal mode
 */
static int zeal_normal_mode_run(zeal_t* machine)
{
    const int elapsed_tstates = z80_step(&machine->cpu);
    /* Go through all the devices that have a tick function */
    zvb_tick(&machine->zvb, elapsed_tstates);
    /* Send keyboard keys to Zeal MV only if its window is focused */
    zeal_read_keyboard(machine, elapsed_tstates);

    if (zvb_prepare_render(&machine->zvb)) {
        const int screen_w = GetScreenWidth();
        const int screen_h = GetScreenHeight();
        const float texture_ratio = (float)ZVB_MAX_RES_WIDTH / ZVB_MAX_RES_HEIGHT;
        const float screen_ratio  = (float)screen_w / screen_h;

        int pos_x = 0;
        int pos_y = 0;

        int draw_w = ZVB_MAX_RES_WIDTH;
        int draw_h = ZVB_MAX_RES_HEIGHT;

        if (texture_ratio > screen_ratio) {
            /* Texture is "wider" than the screen, add bars on top/bottom */
            draw_w = screen_w;
            draw_h = (int)(screen_w / texture_ratio);
            pos_y = (screen_h - draw_h) / 2;
        } else {
            /* Texture is "taller" than the screen, add bars on left/right */
            draw_h = screen_h;
            draw_w = (int)(screen_h * texture_ratio);
            pos_x = (screen_w - draw_w) / 2;
        }

        BeginTextureMode(machine->zvb_out);
            zvb_render(&machine->zvb);
        EndTextureMode();

        BeginDrawing();
            ClearBackground(DARKGRAY);
            DrawTexturePro(machine->zvb_out.texture,
                            (Rectangle){ 0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT },
                            (Rectangle){ pos_x, pos_y, draw_w, draw_h },
                            (Vector2){ 0, 0 },
                            0.0f,
                            WHITE);
        EndDrawing();
    }
    return 0;
}


int zeal_run(zeal_t* machine)
{
    int ret = 0;
    bool debug_key_pressed = false;

    if (machine == NULL) {
        return -1;
    }

    bool running = true;
    while (running) {
        if(WindowShouldClose()) {
            config_window_update(machine->dbg_enabled);
            running = false;
            break;
        }

        if(!debug_key_pressed && IsKeyPressed(KEY_F12)) {
            debug_key_pressed = true;
            zeal_debug_toggle(machine);
        } else if(IsKeyReleased(KEY_F12)) {
            debug_key_pressed = false;
        }

        if (machine->dbg_enabled) {
            ret = zeal_dbg_mode_run(machine);
        } else {
            ret = zeal_normal_mode_run(machine);
        }
        if(ret != 0) {
            /* TODO: Process emulated program request */
        }
    }

    if(machine->dbg_ui != NULL) {
        debugger_ui_deinit(machine->dbg_ui);
    }

    zvb_deinit(&machine->zvb);
    CloseWindow();

    return ret;
}

