#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/zeal.h"
#include "utils/log.h"
#include "utils/config.h"

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


static void zeal_read_keyboard(zeal_t* machine, int delta) {
    static kb_keys_t RAYLIB_KEYS[RAYLIB_KEY_COUNT];

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

    keyboard_send_next(&machine->keyboard, &machine->pio, delta);
}


int zeal_debug_enable(zeal_t* machine)
{
    config_window_update(machine->dbg_enabled);
    int ret = 0;
    machine->dbg_enabled = true;
    machine->dbg_state = ST_PAUSED;
    config_window_set(true);
    if(machine->dbg_ui == NULL) {
        ret = debugger_ui_init(&machine->dbg_ui, &machine->zvb_out);
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


static void zeal_debug_toggle(dbg_t *dbg)
{
    if (dbg == NULL) return;
    zeal_t* machine = (zeal_t*) (dbg->arg);

    if (machine->dbg_enabled) {
        log_printf("zeal_debug_toggle: disable\n");
        zeal_debug_disable(machine);
    } else {
        log_printf("zeal_debug_toggle: enable\n");
        zeal_debug_enable(machine);
    }
}

static memory_op_t s_ops = {
    .read_byte = zeal_mem_read,
    .write_byte = zeal_mem_write,
    .phys_read_byte = zeal_phys_mem_read,
    .phys_write_byte = zeal_phys_mem_write,
};

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
    machine->dbg.running = true;
    /* Set the debug mode in the machine structure as soon as possible */
    machine->dbg_enabled = config_debugger_enabled();


    /* Initialize the UI. It must be done before any shader is created! */
    SetTraceLogLevel(WIN_LOG_LEVEL);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    /* initialize raylib window */
    InitWindow(0, 0, WIN_NAME);
    config_window_set(machine->dbg_enabled);
    SetExitKey(KEY_NULL);
    SetWindowFocused(); // force focus on the window to capture keypresses

#if !BENCHMARK
    SetTargetFPS(60);
#endif

    /* Since we want to enable scaling, make the ZVB output always go to a texture first */
    machine->zvb_out = LoadRenderTexture(ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT);

    /* initialize the debugger */
    zeal_debugger_init(machine, &machine->dbg);
    /* Load symbols if provided */
    if (config.arguments.map_file) {
        debugger_load_symbols(&machine->dbg, config.arguments.map_file);
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
    err = zvb_init(&machine->zvb, false, &s_ops);
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
    err = compactflash_init(&machine->compactflash, config.arguments.cf_filename);

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
    zeal_add_io_device(machine, 0x70, &machine->compactflash.parent);
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
static int zeal_normal_mode_run(zeal_t* machine, bool ignore_keyboard)
{
    const int elapsed_tstates = z80_step(&machine->cpu);
    /* Go through all the devices that have a tick function */
    zvb_tick(&machine->zvb, elapsed_tstates);
    /* Send keyboard keys to Zeal MV only if its window is focused */
    if(!ignore_keyboard) zeal_read_keyboard(machine, elapsed_tstates);

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




void main_scale_up(dbg_t *dbg) {
    (void)dbg; // unreferenced
    int width = GetScreenWidth();
    Vector2 size = config_get_next_resolution(width);
    SetWindowSize(size.x, size.y);
}
void main_scale_down(dbg_t *dbg) {
    (void)dbg; // unreferenced
    int width = GetScreenWidth();
    Vector2 size = config_get_prev_resolution(width);
    SetWindowSize(size.x, size.y);
}

typedef struct {
    bool pressed; // TODO: support key repeat with GetTime()??
    bool shifted;
    const char *label;
    KeyboardKey key;
    debugger_callback_t callback;
} debugger_key_t;

debugger_key_t debugger_key_toggle = { .label = "Toggle Debugger", .key = KEY_F1, .callback = zeal_debug_toggle, .pressed = false, .shifted = false };

debugger_key_t main_keys[] = {
    { .label = "Scale Up", .key = KEY_EQUAL, .callback = main_scale_up, .pressed = false, .shifted = true },
    { .label = "Scale Down", .key = KEY_MINUS, .callback = main_scale_down, .pressed = false, .shifted = true },
};
int main_keys_size = sizeof(main_keys) / sizeof(debugger_key_t);

debugger_key_t debugger_keys[] = {
    // { .label = "Toggle Debugger", .key = KEY_F1, .callback = zeal_debug_toggle, .pressed = false, .shifted = false },
    { .label = "Pause", .key = KEY_F6, .callback = debugger_pause, .pressed = false, .shifted = false },
    { .label = "Continue", .key = KEY_F5, .callback = debugger_continue, .pressed = false, .shifted = false },
    { .label = "Restart", .key = KEY_F5, .callback = debugger_restart, .pressed = false, .shifted = true },
    { .label = "Step Over", .key = KEY_F10, .callback = debugger_step_over, .pressed = false, .shifted = false },
    { .label = "Step", .key = KEY_F11, .callback = debugger_step, .pressed = false, .shifted = false },
    { .label = "Toggle Breakpoint", .key = KEY_F9, .callback = debugger_breakpoint, .pressed = false, .shifted = false },
    { .label = "Scale Up", .key = KEY_EQUAL, .callback = debugger_scale_up, .pressed = false, .shifted = true },
    { .label = "Scale Down", .key = KEY_MINUS, .callback = debugger_scale_down, .pressed = false, .shifted = true },
};
int debugger_keys_size = sizeof(debugger_keys) / sizeof(debugger_key_t);

bool zeal_ui_input(zeal_t* machine) {
    if(config_keyboard_passthru(machine->dbg_enabled)) return false;
    bool handled = false;

    // Debugger UI requires Ctrl + {KEY}
    #ifdef __APPLE__
    // MacOS has too many default bindings for Ctrl + F* keys
    bool meta = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    #else
    bool meta = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    #endif

    if(!meta) return false; /// all zeal ui keystrokes require meta?

    bool shift = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));

    // toggle between main view and debugger view
    debugger_key_t *opt = &debugger_key_toggle;
    bool pressed = IsKeyDown(opt->key);
    handled = handled || (pressed);
    if(meta && !opt->pressed && pressed) {
        zeal_debug_toggle(&machine->dbg);
        opt->pressed = true;
    } else if(!pressed) {
        opt->pressed = false;
    }

    int keys_size;
    debugger_key_t *keys;
    if(machine->dbg_enabled) {
        keys = debugger_keys;
        keys_size = debugger_keys_size;
    } else {
        keys_size = main_keys_size;
        keys = main_keys;
    }

    // debugger ui
    for(int i = 0; i < keys_size; i++) {
        opt = &keys[i];
        bool shifted = (opt->shifted == shift);
        pressed = IsKeyDown(opt->key);
        handled = handled || (pressed && shifted);
        if(shifted && !opt->pressed && pressed) {
            opt->callback(&machine->dbg);
            opt->pressed = true;
        } else if(!pressed) {
            opt->pressed = false;
        }
    };

    return handled;
}

int zeal_run(zeal_t* machine)
{
    int ret = 0;

    if (machine == NULL) {
        return -1;
    }

    while (!WindowShouldClose()) {
        if(!machine->dbg.running) break;

        bool input_handled = zeal_ui_input(machine);

        if (machine->dbg_enabled) {
            ret = zeal_dbg_mode_run(machine);
        } else {
            ret = zeal_normal_mode_run(machine, input_handled);
        }
        if(ret != 0) {
            /* TODO: Process emulated program request */
        }
    }

    config_window_update(machine->dbg_enabled);

    if(machine->dbg_ui != NULL) {
        debugger_ui_deinit(machine->dbg_ui);
    }

    zvb_deinit(&machine->zvb);

    CloseWindow();

    return ret;
}

