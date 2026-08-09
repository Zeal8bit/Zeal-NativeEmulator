/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
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
#include "utils/notif.h"
#include "utils/vtimer.h"
#ifdef PLATFORM_WEB
#include <emscripten.h>
#endif

#define RAYLIB_KEY_COUNT    384

/**
 * @brief Period, in T-states, between host keyboard polls (15ms).
 */
#define HOST_KEYB_CHECK_PERIOD   US_TO_TSTATES(15000)

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

static void host_keyboard_check_cb(void* userdata);

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





#if CONFIG_ENABLE_DEBUGGER
/**
 * @brief Read a byte from memory for the debugger, so write-only areas can still be read
 */
static uint8_t debug_read_memory(zeal_t* machine, hwaddr virt_addr)
{
    const int phys_addr      = mmu_get_phys_addr(&machine->cpu.mmu, virt_addr);
    const map_entry_t* entry = &machine->cpu.mmu.mem_mapping[phys_addr / MMU_PAGE_SIZE];
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


int zeal_reset(zeal_t* machine)
{
    z80_reset(&machine->cpu);
    if (!machine->headless) {
        zeal_read_keyboard_reset(machine);
    }
    device_reset(DEVICE(&machine->cpu.mmu));
    device_reset(DEVICE(&machine->pio));
    device_reset(DEVICE(&machine->keyboard));
    device_reset(DEVICE(&machine->zvb));

    /* If a user program was specified, re-read the ROM and re-inject it so that
     * recompiled programs are picked up automatically on reset */
    if (config.arguments.uprog_filename != NULL) {
        flash_load_from_file(&machine->rom, config.arguments.rom_filename, config.arguments.uprog_filename);
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

    memset(machine, 0, sizeof(*machine));
    vtimer_init();
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

        SetTargetFPS(60);
        notif_reset();

        /* Force rendering the window, to allow Raylib periphs to attach (ie; gamepads) */
        BeginDrawing();
            ClearBackground(BLACK);
        EndDrawing();

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

    z80_init(&machine->cpu);
    mmu_t* mmu = z80_get_mmu(&machine->cpu);

    err = flash_init(&machine->rom);
    CHECK_ERR(err);

    err = ram_init(&machine->ram);
    CHECK_ERR(err);

    err = pio_init(machine, &machine->pio);
    CHECK_ERR(err);

    err = uart_init(&machine->uart, &machine->pio);
    CHECK_ERR(err);

    err = i2c_init(&machine->i2c_bus, &machine->pio);
    CHECK_ERR(err);

    err = keyboard_init(&machine->keyboard, &machine->pio);
    CHECK_ERR(err);
    vtimer_init_node(&machine->host_keyb_timer, host_keyboard_check_cb, machine);
    vtimer_schedule_tstates(&machine->host_keyb_timer, HOST_KEYB_CHECK_PERIOD);

    err = snes_adapter_init(&machine->snes_adapter, &machine->pio);
    CHECK_ERR(err);

    err = ds1307_init(&machine->rtc);
    CHECK_ERR(err);

    err = i2c_connect(&machine->i2c_bus, &machine->rtc.parent);
    CHECK_ERR(err);

    const int cf_err = compactflash_init(&machine->compactflash, config.arguments.cf_filename);

    err = at24c512_init(&machine->eeprom, config.arguments.eeprom_filename);
    CHECK_ERR(err);

    err = i2c_connect(&machine->i2c_bus, &machine->eeprom.parent);
    CHECK_ERR(err);

    err = hostfs_init(&machine->hostfs, mmu);
    CHECK_ERR(err);

    /* Initialize the semihosting device with CPU pointer for register access */
    err = semihost_init(&machine->semihost, &machine->cpu);
    CHECK_ERR(err);

    /* Register the devices in the memory space */
    mmu_register_mem_device(mmu, 0x000000, &machine->rom.parent);
    if (machine->rom.size < NOR_FLASH_SIZE_KB_MAX) {
        /* Create a mirror in the upper 256KB */
        mmu_register_mem_device(mmu, 0x040000, &machine->rom.parent);
    }

    mmu_register_mem_device(mmu, 0x080000, &machine->ram.parent);
    const zvb_config_t zvb_config = {
        .rendering_enabled = !machine->headless,
    };
    err = zvb_init(&machine->zvb, &zvb_config, mmu);
    CHECK_ERR(err);
    if (!machine->headless) {
        SetMasterVolume(config.audio.volume / 100.0f);
        if (config.audio.volume != 100) {
            notif_show("Volume: %d%%", config.audio.volume);
        }
    }
    mmu_register_mem_device(mmu, 0x100000, &machine->zvb.parent);

    /* Register the devices in the I/O space */
    mmu_register_io_device(mmu, 0x10, &machine->semihost.parent);
    if (cf_err == 0) {
        mmu_register_io_device(mmu, 0x70, &machine->compactflash.parent);
    }
    mmu_register_io_device(mmu, 0x80, &machine->zvb.parent);
    mmu_register_io_device(mmu, 0xc0, &machine->hostfs.parent);
    mmu_register_io_device(mmu, 0xd0, &machine->pio.parent);
    mmu_register_io_device(mmu, 0xe0, &machine->keyboard.parent);
    mmu_register_io_device(mmu, 0xf0, &machine->cpu.mmu.parent);

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


/**
 * @brief Periodic vtimer callback to poll the host keyboard for new key events.
 */
static void host_keyboard_check_cb(void* userdata)
{
    zeal_t* machine = (zeal_t*) userdata;

    /* Reschedule for the next periodic check */
    vtimer_schedule_tstates(&machine->host_keyb_timer, HOST_KEYB_CHECK_PERIOD);

#if CONFIG_ENABLE_DEBUGGER
    /* Skip polling if CPU is paused in debug mode */
    if (machine->dbg_enabled && machine->dbg_state != ST_RUNNING) {
        return;
    }
    /* Skip if a UI shortcut consumed the input */
    if (zeal_ui_input(machine)) {
        return;
    }
    /* Skip if the debugger main view doesn't have focus */
    if (machine->dbg_ui != NULL && !debugger_ui_main_view_focused(machine->dbg_ui)) {
        return;
    }
#endif
    zeal_read_keyboard(machine, HOST_KEYB_CHECK_PERIOD);
}


/**
 * @brief Run Zeal 8-bit Computer VM in headless mode (no window/input/presentation)
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

    vtimer_tick(elapsed_tstates);
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
            .main_view = &machine->zvb.blitter.main_texture,
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
#if CONFIG_PROFILE_RENDER
    const double profile_start = GetTime();
#endif

    /**
     * Prepare the rendering, if the returned value is true, we can
     * proceed to rendering, else, we don't need to update the view.
     * However, if the CPU is paused (breakpoint/step), force the rendering.
     */
    if (zvb_prepare_render(&machine->zvb)) {
        /* Display all the devices that have a render function */
        zvb_render(&machine->zvb);
    } else if (machine->dbg_state == ST_PAUSED) {
        zvb_force_render(&machine->zvb);
    } else {
        /* Do not proceed, the CPU is currently running and the ZVB doens't need to be refreshed yet */
        return 0;
    }

    /* Update only the VRAM debug view currently focused in the panel */
    const dbg_vram_t debug_view = debugger_ui_vram_panel_opened(machine->dbg_ui);
    if (debug_view != DBG_VIEW_NONE) {
        zvb_render_debug_textures(&machine->zvb, debug_view);
    }

    debugger_ui_prepare_render(machine->dbg_ui, &machine->dbg);
    BeginDrawing();
        /* Grey brackground */
        ClearBackground((Color){ 0x63, 0x63, 0x63, 0xff });
        debugger_ui_render(machine->dbg_ui, &machine->dbg);
        notif_render(GetScreenWidth() - notif_estimate_width() - 20, 10);
        if(show_fps == true) {
            DrawFPS(10, 10);
        }

    EndDrawing();

#if CONFIG_PROFILE_RENDER
    zvb_profile_frame(GetTime() - profile_start);
#endif

    return 1;
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

        vtimer_tick(elapsed_tstates);

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

    /* Check the next event and run for that many ticks */
    // const uint64_t next_event = vtimer_next_event();
    // unsigned long ran_for = z80_run_for(&machine->cpu, next_event);
    // vtimer_tick(ran_for);

    const int elapsed_tstates = z80_step(&machine->cpu);
    if (config.arguments.no_reset && machine->cpu.pc == 0) {
        /* PC is back to 0, that's a software reset!
         * Return 2 to tell the caller we rendered 2 frames, forcing it to exit the current loop and
         * check for the close/exit flag */
        log_printf("[ZEAL] PC returned to 0x0000 after running (cyc=%lu), exiting\n", machine->cpu.cyc);
        zeal_exit(machine);
        return 2;
    }
    vtimer_tick(elapsed_tstates);

    if (zvb_prepare_render(&machine->zvb)) {
        rendered = 1;
#if CONFIG_PROFILE_RENDER
        const double profile_start = GetTime();
#endif
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

        zvb_render(&machine->zvb);

        BeginDrawing();
            ClearBackground(DARKGRAY);
            DrawTexturePro(zvb_output_texture(&machine->zvb),
                            (Rectangle){ 0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT },
                            (Rectangle){ pos_x, pos_y, draw_w, draw_h },
                            (Vector2){ 0, 0 },
                            0.0f,
                            WHITE);
            /* Show notifications on the top-right of the visible content */
            notif_render(pos_x + draw_w - notif_estimate_width() - 20, pos_y + 10);
            if(show_fps == true) {
                DrawFPS(10, 10);
            }
        EndDrawing();

#if CONFIG_PROFILE_RENDER
        zvb_profile_frame(GetTime() - profile_start);
#endif
    }
    return rendered;
}

static void zeal_loop(zeal_t* machine)
{
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

        int frame_rendered = 0;
#if CONFIG_ENABLE_DEBUGGER
        if (machine->dbg_enabled) {
            frame_rendered = zeal_dbg_mode_run(machine);
        } else
#endif // CONFIG_ENABLE_DEBUGGER
        {
            frame_rendered = zeal_normal_mode_run(machine);
        }
        rendered += frame_rendered;
        if (frame_rendered > 0) {
            snes_adapter_update(&machine->snes_adapter);
        }
    }
}

static void zeal_run_headless(zeal_t* machine)
{
    while (!machine->should_exit) {
        zeal_headless_mode_run(machine);
        if (config.arguments.headless_run_ticks > 0 && machine->cpu.cyc >= config.arguments.headless_run_ticks) {
            log_printf("[ZEAL] Ran for %lu ticks\n", machine->cpu.cyc);
            break;
        }
    }

    snes_adapter_detach(&machine->snes_adapter);
    zvb_deinit(&machine->zvb);
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

    if (machine->headless) {
        zeal_run_headless(machine);
        return 0;
    }

    while (!machine->should_exit && !WindowShouldClose()) {
#if CONFIG_ENABLE_DEBUGGER
        if(!machine->dbg.running) {
            break;
        }
#endif // CONFIG_ENABLE_DEBUGGER
        zeal_loop(machine);
    }

#if CONFIG_ENABLE_DEBUGGER
    config_window_update(machine->dbg_enabled);

    if(machine->dbg_ui != NULL) {
        debugger_ui_deinit(machine->dbg_ui);
    }
#else
        config_window_update(false);
#endif // CONFIG_ENABLE_DEBUGGER

    snes_adapter_detach(&machine->snes_adapter);
    zvb_deinit(&machine->zvb);
    CloseWindow();

    return ret;
}
