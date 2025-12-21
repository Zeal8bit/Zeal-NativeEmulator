/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/zeal.h"
#include "utils/log.h"
#include "utils/config.h"
#ifdef PLATFORM_WEB
#include <emscripten.h>
#endif

#define RAYLIB_KEY_COUNT    384

#define CHECK_ERR(err)  \
    do {                \
        if (err)        \
            return err; \
    } while (0)


typedef enum {
    KEY_NOT_PRESSED,
    KEY_PRESSED,
    KEY_REPEATED,
} kb_key_state_t;

typedef struct {
    kb_key_state_t state;
    int duration;
} kb_keys_t;


int zeal_debugger_init(zeal_t* machine, dbg_t* dbg);
bool zeal_ui_input(zeal_t* machine);

/**
 * @brief Array used to key tracked of the key states on the host. This array will help simulate
 * key press, release and repeat.
 */
static kb_keys_t RAYLIB_KEYS[RAYLIB_KEY_COUNT];


#ifdef PLATFORM_WEB
EMSCRIPTEN_KEEPALIVE volatile
#endif
#if CONFIG_SHOW_FPS
bool show_fps = true;
#else
bool show_fps = false;
#endif


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

    log_printf("[INFO] No device replied to memory read: 0x%04x (PC @ 0x%04x)\n", phys_addr, machine->cpu.pc);
    return 0;
}

/**
 * @brief Read a byte from memory given a physical address
 */
static uint8_t zeal_phys_mem_read(void* opaque, uint32_t phys_addr)
{
    if (phys_addr >= MEM_SPACE_SIZE) {
        log_printf("[INFO] Invalid physical address memory read: 0x%04x\n", phys_addr);
        return 0;
    }
    const zeal_t* machine    = (zeal_t*) opaque;
    const map_entry_t* entry = &machine->mem_mapping[phys_addr / MMU_PAGE_SIZE];
    device_t* device         = entry->dev;
    const int start_addr     = entry->page_from * MMU_PAGE_SIZE;

    if (device) {
        return device->mem_region.read(device, phys_addr - start_addr);
    }

    log_printf("[INFO] No device replied to physical memory read: 0x%04x\n", phys_addr);
    return 0;
}


#if CONFIG_ENABLE_DEBUGGER
/**
 * @brief Read a byte from memory for the debugger, so write-only areas can still be read
 */
static uint8_t debug_read_memory(zeal_t* machine, hwaddr virt_addr)
{
    const int phys_addr      = mmu_get_phys_addr(&machine->mmu, virt_addr);
    const map_entry_t* entry = &machine->mem_mapping[phys_addr / MMU_PAGE_SIZE];
    device_t* device         = entry->dev;
    const int start_addr     = entry->page_from * MMU_PAGE_SIZE;

    if (device) {
        return device->mem_region.debug_read ?
                    device->mem_region.debug_read(device, phys_addr - start_addr) :
                    device->mem_region.read(device, phys_addr - start_addr);
    }

    log_printf("[INFO] No device replied to memory read: 0x%04x (PC @ 0x%04x)\n", phys_addr, machine->cpu.pc);
    return 0;
}
#endif


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
        log_printf("[INFO] No device replied to memory write: 0x%04x\n", phys_addr);
    }
}

/**
 * @brief Write a byte to memory given a physical address
 */
static void zeal_phys_mem_write(void* opaque, uint32_t phys_addr, uint8_t data)
{
    if (phys_addr >= MEM_SPACE_SIZE) {
        log_printf("[INFO] Invalid physical address memory write: 0x%04x\n", phys_addr);
        return;
    }
    const zeal_t* machine    = (zeal_t*) opaque;
    const map_entry_t* entry = &machine->mem_mapping[phys_addr / MMU_PAGE_SIZE];
    device_t* device         = entry->dev;
    const int start_addr     = entry->page_from * MMU_PAGE_SIZE;

    if (device) {
        device->mem_region.write(device, phys_addr - start_addr, data);
    } else {
        log_printf("[INFO] No device replied to physical memory write: 0x%04x\n", phys_addr);
    }
}

static uint8_t zeal_io_read(void* opaque, uint16_t addr)
{
    zeal_t* machine          = (zeal_t*) opaque;
    const int low            = addr & 0xff;
    const map_entry_t* entry = &machine->io_mapping[low];
    device_t* device         = entry->dev;

    if (device && device->io_region.read) {
        device->io_region.upper_addr = addr >> 8;
        return device->io_region.read(device, low - entry->page_from);
    }

    log_printf("[INFO] No device replied to I/O read: 0x%04x\n", low);
    return 0;
}

static void zeal_io_write(void* opaque, uint16_t addr, uint8_t data)
{
    zeal_t* machine          = (zeal_t*) opaque;
    const int low            = addr & 0xff;
    const map_entry_t* entry = &machine->io_mapping[low];
    device_t* device         = entry->dev;

    if (device && device->io_region.write) {
        device->io_region.write(device, low - entry->page_from, data);
    } else {
        log_printf("[INFO] No device replied to I/O write: 0x%04x\n", low);
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
        log_err_printf("%s: cannot register device, invalid region 0x%02x (%d bytes)\n", __func__, region_start, region_size);
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
        log_err_printf("%s: cannot register device, invalid region 0x%02x (%d bytes)\n", __func__, region_start, region_size);
        return;
    }

    /* Make sure the alignment is correct too! */
    if ((region_start & (MEM_SPACE_ALIGN - 1)) != 0 || (region_size & (MEM_SPACE_ALIGN - 1)) != 0) {
        log_err_printf("%s: cannot register device, invalid alignment for region 0x%02x (%d bytes)\n", __func__, region_start,
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
            log_err_printf("%s: cannot register device %s in page %d, device %s is already mapped\n",
                __func__, dev->name, page, entry->dev->name);
        }
        *entry = (map_entry_t) {.dev = dev, .page_from = start_page};
    }
}


static int key_can_repeat(int code)
{
    const int modifiers[] = {
        KEY_LEFT_SHIFT,  KEY_LEFT_CONTROL,  KEY_LEFT_ALT,  KEY_LEFT_SUPER,
        KEY_RIGHT_SHIFT, KEY_RIGHT_CONTROL, KEY_RIGHT_ALT, KEY_RIGHT_SUPER,
        KEY_CAPS_LOCK,   KEY_NUM_LOCK
    };

    for (unsigned int i = 0; i < DIM(modifiers); i++) {
        if (modifiers[i] == code) {
            return false;
        }
    }
    return true;
}


static void zeal_read_keyboard_reset(zeal_t* machine)
{
    (void) machine;
    /* Clear Raylib's key states */
    while(GetKeyPressed()) {}

    for (int i = 0; i < RAYLIB_KEY_COUNT; i++) {
        RAYLIB_KEYS[i].duration = 0;
        RAYLIB_KEYS[i].state = KEY_NOT_PRESSED;
    }
}

static void zeal_read_keyboard(zeal_t* machine, int delta)
{
    int keyCode;

    /* The initial delay is ~500ms before repeat starts */
    const int start_delay = us_to_tstates(500000);
    /* Then, repeat the key every 50ms */
    const int repeat_delay = us_to_tstates(50000);

    // look for newly pressed keys
    while((keyCode = GetKeyPressed())) {
        RAYLIB_KEYS[keyCode].state = KEY_PRESSED;
        RAYLIB_KEYS[keyCode].duration = 0;
        key_pressed(&machine->keyboard, keyCode);
    }

    // look for newly released keys
    for(keyCode = 0; keyCode < RAYLIB_KEY_COUNT; keyCode++) {
        kb_keys_t* key = &RAYLIB_KEYS[keyCode];

        if(key->state == KEY_NOT_PRESSED) {
            continue;
        }

        if(IsKeyUp(keyCode)) {
            key->state = KEY_NOT_PRESSED;
            /* No need to clear the duration, it's done when the key is pressed */
            key_released(&machine->keyboard, keyCode);
            continue;
        }

        /* Key is still pressed, add the current delta to its duration and check it */
        key->duration += delta;

        if (key->state == KEY_PRESSED && key_can_repeat(keyCode) && key->duration >= start_delay) {
            key_pressed(&machine->keyboard, keyCode);
            key->state = KEY_REPEATED;
            key->duration = 0;
        } else if (key->state == KEY_REPEATED && key->duration >= repeat_delay) {
            key->duration = 0;
            key_pressed(&machine->keyboard, keyCode);
        }
    }
}


static memory_op_t s_ops = {
    .read_byte = zeal_mem_read,
    .write_byte = zeal_mem_write,
    .phys_read_byte = zeal_phys_mem_read,
    .phys_write_byte = zeal_phys_mem_write,
};

int zeal_reset(zeal_t* machine) {
    zeal_init_cpu(machine);
    if (!machine->headless) {
        zeal_read_keyboard_reset(machine);
    }
    device_reset(DEVICE(&machine->mmu));
    device_reset(DEVICE(&machine->keyboard));
    if (!machine->headless) {
        device_reset(DEVICE(&machine->zvb));
    }

#if CONFIG_ENABLE_DEBUGGER
    if(machine->dbg_enabled) {
        machine->dbg_state = ST_PAUSED;
    }
#endif // CONFIG_ENABLE_DEBUGGER
    return 0;
}

int zeal_init(zeal_t* machine)
{
    int err = 0;
    if (machine == NULL) {
        return 1;
    }

    /* It wouldn't make sense to have two machines now... */
    if (s_ops.opaque != NULL) {
        log_err_printf("[ZEAL] ERROR: machine already initialized\n");
        return 2;
    }
    s_ops.opaque = machine;

    memset(machine, 0, sizeof(*machine));
    machine->headless = config.arguments.headless;
#if CONFIG_ENABLE_DEBUGGER
    machine->dbg_read_memory = debug_read_memory;
    machine->dbg.running = true;
    /* Set the debug mode in the machine structure as soon as possible */
    machine->dbg_enabled = config_debugger_enabled() && !machine->headless;
#endif // CONFIG_ENABLE_DEBUGGER

    if (!machine->headless) {
        /* Initialize the UI. It must be done before any shader is created! */
        SetTraceLogLevel(WIN_LOG_LEVEL);
#ifndef PLATFORM_WEB
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif

        /* initialize raylib window */
        InitWindow(640, 480, WIN_NAME);
        SetExitKey(KEY_NULL);
#ifndef PLATFORM_WEB
        SetWindowFocused(); // force focus on the window to capture keypresses
#endif
#if !BENCHMARK
        SetTargetFPS(60);
#endif

        /* Since we want to enable scaling, make the ZVB output always go to a texture first */
        machine->zvb_out = LoadRenderTexture(ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT);

#if CONFIG_ENABLE_DEBUGGER
        config_window_set(machine->dbg_enabled);
        /* Initialize the debugger */
        zeal_debugger_init(machine, &machine->dbg);
        /* Load symbols if provided */
        if (config.arguments.map_file) {
            debugger_load_symbols(&machine->dbg, config.arguments.map_file);
        }
        if (config.arguments.breakpoints) {
            debugger_set_breakpoints_str(&machine->dbg, config.arguments.breakpoints);
        }
#endif // CONFIG_ENABLE_DEBUGGER
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

    // const uart = new UART(this, pio);
    err = uart_init(&machine->uart, &machine->pio);
    CHECK_ERR(err);
    // const uart_web_serial = new UART_WebSerial(this, pio);

    // const i2c = new I2C(this, pio);
    err = i2c_init(&machine->i2c_bus, &machine->pio);
    CHECK_ERR(err);

    // const keyboard = new Keyboard(this, pio);
    err = keyboard_init(&machine->keyboard, &machine->pio);
    CHECK_ERR(err);

    // const ds1307 = new I2C_DS1307(this, i2c);
    err = ds1307_init(&machine->rtc);
    CHECK_ERR(err);

    err = i2c_connect(&machine->i2c_bus, &machine->rtc.parent);
    CHECK_ERR(err);

    // /* Extensions */
    // const compactflash = new CompactFlash(this);
    const int cf_err = compactflash_init(&machine->compactflash, config.arguments.cf_filename);

    // /* We could pass an initial content to the EEPROM, but set it to null for the moment */
    // const eeprom = new I2C_EEPROM(this, i2c, null);
    err = at24c512_init(&machine->eeprom, config.arguments.eeprom_filename);
    CHECK_ERR(err);

    err = i2c_connect(&machine->i2c_bus, &machine->eeprom.parent);
    CHECK_ERR(err);

    // /* Create a HostFS to ease the file and directory access for the VM */
    // const hostfs = new HostFS(this.mem_read, this.mem_write);
    err = hostfs_init(&machine->hostfs, &s_ops);
    CHECK_ERR(err);

    /* Register the devices in the memory space */
    zeal_add_mem_device(machine, 0x000000, &machine->rom.parent);
    if (machine->rom.size < NOR_FLASH_SIZE_KB_MAX) {
        /* Create a mirror in the upper 256KB */
        zeal_add_mem_device(machine, 0x040000, &machine->rom.parent);
    }

    zeal_add_mem_device(machine, 0x080000, &machine->ram.parent);
    if (!machine->headless) {
        err = zvb_init(&machine->zvb, false, &s_ops);
        CHECK_ERR(err);
        zeal_add_mem_device(machine, 0x100000, &machine->zvb.parent);
    }

    /* Register the devices in the I/O space */
    if (cf_err == 0) {
        zeal_add_io_device(machine, 0x70, &machine->compactflash.parent);
    }
    if (!machine->headless) {
        zeal_add_io_device(machine, 0x80, &machine->zvb.parent);
    }
    zeal_add_io_device(machine, 0xc0, &machine->hostfs.parent);
    zeal_add_io_device(machine, 0xd0, &machine->pio.parent);
    zeal_add_io_device(machine, 0xe0, &machine->keyboard.parent);
    zeal_add_io_device(machine, 0xf0, &machine->mmu.parent);

#if CONFIG_ENABLE_DEBUGGER
    /* Since the debugger may depend on some components, make sure they are all initialized */
    if (machine->dbg_enabled) {
        zeal_debug_enable(machine);
        /* Force the machine in RUNNING mode */
        machine->dbg_state = ST_RUNNING;
    }
#endif // CONFIG_ENABLE_DEBUGGER

    return 0;
}

#if CONFIG_ENABLE_DEBUGGER
int zeal_debug_enable(zeal_t* machine)
{
    if (machine->headless) {
        return -1;
    }
    config_window_update(machine->dbg_enabled);
    int ret = 0;
    machine->dbg_enabled = true;
    machine->dbg_state = ST_PAUSED;
    config_window_set(true);
    if(machine->dbg_ui == NULL) {
        dbg_ui_init_args_t args = {
            .main_view = &machine->zvb_out,
            .zvb = &machine->zvb,
        };
        args.debug_views = zvb_get_debug_textures(&machine->zvb, &args.debug_views_count);
        ret = debugger_ui_init(&machine->dbg_ui, &args);
    }
    return ret;
}


int zeal_debug_disable(zeal_t* machine)
{
    config_window_update(machine->dbg_enabled);
    machine->dbg_enabled = false;
    machine->dbg_state = ST_RUNNING;
    config_window_set(false);
    return 0;
}


void zeal_debug_toggle(dbg_t *dbg)
{
    if (dbg == NULL) return;
    zeal_t* machine = (zeal_t*) (dbg->arg);

    if (machine->dbg_enabled) {
        log_printf("[DEBUGGER]: Disabled\n");
        zeal_debug_disable(machine);
    } else {
        log_printf("[DEBUGGER]: Enabled\n");
        zeal_debug_enable(machine);
    }
}

/**
 * Returns 1 if rendered, 0 else
 */
static int zeal_dbg_mode_display(zeal_t* machine)
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
        return 0;
    }

    /* Generate VRAM debug textures if the VRAM debugging window is opened */
    if (debugger_ui_vram_panel_opened(machine->dbg_ui)) {
        zvb_render_debug_textures(&machine->zvb);
    }

    debugger_ui_prepare_render(machine->dbg_ui, &machine->dbg);
    BeginDrawing();
        /* Grey brackground */
        ClearBackground((Color){ 0x63, 0x63, 0x63, 0xff });
        debugger_ui_render(machine->dbg_ui, &machine->dbg);
    if(show_fps == true) {
        DrawFPS(10, 10);
    }

    EndDrawing();

    return 1;
}


/**
 * @brief Run Zeal 8-bit Computer VM in headless mode (no rendering/input)
 */
static int zeal_headless_mode_run(zeal_t* machine)
{
    const int elapsed_tstates = z80_step(&machine->cpu);
    if (config.arguments.no_reset && machine->cpu.pc == 0) {
        /* PC is back to 0, that's a software reset! */
        log_printf("[ZEAL] PC returned to 0x0000 after running (cyc=%lu), exiting\n", machine->cpu.cyc);
        zeal_exit(machine);
        return 0;
    }

    keyboard_tick(&machine->keyboard, &machine->pio, elapsed_tstates);
    flash_tick(&machine->rom, elapsed_tstates);
    return 0;
}


/**
 * @brief Run Zeal 8-bit Computer VM in debug mode
 */
static int zeal_dbg_mode_run(zeal_t* machine)
{
    if (machine->dbg_state != ST_PAUSED) {

        if (machine->dbg_state == ST_REQ_STEP_OVER) {
            int instr_size = z80_instruction_size(&machine->cpu);
            debugger_set_temporary_breakpoint(&machine->dbg, machine->cpu.pc + instr_size);
            machine->dbg_state = ST_RUNNING;
        }

        const int elapsed_tstates = z80_step(&machine->cpu);

        /* Check if we need to poll the keyboard and transmit the data to the VM */
        if (keyboard_check(&machine->keyboard, elapsed_tstates) &&
            /* make sure the current keys are not a UI shortcut and the main view is focused */
            !zeal_ui_input(machine) && debugger_ui_main_view_focused(machine->dbg_ui))
        {
            zeal_read_keyboard(machine, elapsed_tstates);
        }

        /* Go through all the devices that have a tick function */
        zvb_tick(&machine->zvb, elapsed_tstates);
        keyboard_tick(&machine->keyboard, &machine->pio, elapsed_tstates);
        flash_tick(&machine->rom, elapsed_tstates);

        /* Check if we reached a breakpoint or if we have to do a single step */
        if (machine->dbg_state == ST_REQ_STEP ||
            debugger_is_breakpoint_set(&machine->dbg, machine->cpu.pc))
        {
            machine->dbg_state = ST_PAUSED;
            debugger_clear_breakpoint_if_temporary(&machine->dbg, machine->cpu.pc);
        }
    }

    int rendered = zeal_dbg_mode_display(machine);
    /* If the CPU is paused, we didn't check the UI input! Check it once per frame */
    if (rendered && machine->dbg_state == ST_PAUSED) {
        zeal_ui_input(machine);
    }
    return rendered;
}
#endif // CONFIG_ENABLE_DEBUGGER


/**
 * @brief Run Zeal 8-bit Computer VM in normal mode.
 *
 * Returns 1 if the screen was rendered, 0 else
 */
static int zeal_normal_mode_run(zeal_t* machine)
{
    int rendered = 0;
    const int elapsed_tstates = z80_step(&machine->cpu);
    if (config.arguments.no_reset && machine->cpu.pc == 0) {
        /* PC is back to 0, that's a software reset!
         * Return 2 to tell the caller we rendered 2 frames, forcing it to exit the current loop and
         * check for the close/exit flag */
        log_printf("[ZEAL] PC returned to 0x0000 after running (cyc=%lu), exiting\n", machine->cpu.cyc);
        zeal_exit(machine);
        return 2;
    }

    /* Send keyboard keys to Zeal VM only if the UI didn't handle it */
    if (keyboard_check(&machine->keyboard, elapsed_tstates)
#if CONFIG_ENABLE_DEBUGGER
        && !zeal_ui_input(machine)
#endif
       ) {
        zeal_read_keyboard(machine, KEYBOARD_CHECK_PERIOD);
    }

    /* Go through all the devices that have a tick function */
    zvb_tick(&machine->zvb, elapsed_tstates);
    keyboard_tick(&machine->keyboard, &machine->pio, elapsed_tstates);
    flash_tick(&machine->rom, elapsed_tstates);

    if (zvb_prepare_render(&machine->zvb)) {
        rendered = 1;
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
        if(show_fps == true) {
            DrawFPS(10, 10);
        }
        EndDrawing();
    }
    return rendered;
}

static void zeal_loop(zeal_t* machine)
{
    if (machine->headless) {
        zeal_headless_mode_run(machine);
        return;
    }
    int rendered = 0;
    /**
     * When compiling for WASM, it is not necessary to execute WindowShouldClose as often as possible.
     * On the contrary, calling it too much would slow the emulation heavily!
     * Calling it once every two rendered frames should be enough to get a stable 60FPS.
     *
     * When compiling as a native appliaction, it is necessarily to call it more often, let's call it
     * once per frame, just like in Raylib examples.
     */
#if PLATFORM_WEB
    while(rendered < 2) {
#else
    while(rendered < 1) {
#endif

#if CONFIG_ENABLE_DEBUGGER
        if (machine->dbg_enabled) {
            rendered += zeal_dbg_mode_run(machine);
        } else
#endif // CONFIG_ENABLE_DEBUGGER
        {
            rendered += zeal_normal_mode_run(machine);
        }
    }
}

void zeal_exit(zeal_t* machine)
{
    machine->should_exit = true;
}

int zeal_run(zeal_t* machine)
{
    int ret = 0;

    if (machine == NULL) {
        return -1;
    }

    while (!machine->should_exit && (machine->headless || !WindowShouldClose())) {
#if CONFIG_ENABLE_DEBUGGER
        if(!machine->dbg.running) {
            break;
        }
#endif // CONFIG_ENABLE_DEBUGGER
        zeal_loop(machine);
    }

#ifdef PLATFORM_WEB
    if (!machine->headless) {
        CloseAudioDevice();
    }
#endif

#if CONFIG_ENABLE_DEBUGGER
    if (!machine->headless) {
        config_window_update(machine->dbg_enabled);

        if(machine->dbg_ui != NULL) {
            debugger_ui_deinit(machine->dbg_ui);
        }
    }
#else
    if (!machine->headless) {
        config_window_update(false);
    }
#endif // CONFIG_ENABLE_DEBUGGER

    if (!machine->headless) {
        zvb_deinit(&machine->zvb);

        CloseWindow();
    }

    return ret;
}
