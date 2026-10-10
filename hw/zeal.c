/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/zeal.h"
#include "host/zeal_host.h"
#include "host/zeal_audio.h"
#include "app/console/console.h"
#include "utils/log.h"
#include "utils/config.h"
#include "utils/notif.h"
#include "utils/vtimer.h"
#ifdef PLATFORM_WEB
#include <emscripten.h>
#endif


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
#if CONFIG_ENABLE_DEBUGGER
#include "hw/debugger/host_frontend.h"
#include "hw/debugger/bindings_internal.h"
#endif

bool zeal_ui_input(zeal_t* machine);

static void host_keyboard_check_cb(void* userdata);

/**
 * @brief Array used to key tracked of the key states on the host. This array will help simulate
 * key press, release and repeat.
 */
static kb_keys_t HOST_KEYS[ZEAL_HOST_KEY_COUNT];


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
        ZEAL_HOST_KEY_LEFT_SHIFT,  ZEAL_HOST_KEY_LEFT_CONTROL,  ZEAL_HOST_KEY_LEFT_ALT,  ZEAL_HOST_KEY_LEFT_SUPER,
        ZEAL_HOST_KEY_RIGHT_SHIFT, ZEAL_HOST_KEY_RIGHT_CONTROL, ZEAL_HOST_KEY_RIGHT_ALT, ZEAL_HOST_KEY_RIGHT_SUPER,
        ZEAL_HOST_KEY_CAPS_LOCK,   ZEAL_HOST_KEY_NUM_LOCK
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
    /* Drop any key presses the host queued while we were not looking. */
    while(zeal_host_key_pressed()) {}

    for (int i = 0; i < ZEAL_HOST_KEY_COUNT; i++) {
        HOST_KEYS[i].duration = 0;
        HOST_KEYS[i].state = KEY_NOT_PRESSED;
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
    while((keyCode = zeal_host_key_pressed())) {
        HOST_KEYS[keyCode].state = KEY_PRESSED;
        HOST_KEYS[keyCode].duration = 0;
        key_pressed(&machine->keyboard, keyCode);
    }

    // look for newly released keys
    for(keyCode = 0; keyCode < ZEAL_HOST_KEY_COUNT; keyCode++) {
        kb_keys_t* key = &HOST_KEYS[keyCode];

        if(key->state == KEY_NOT_PRESSED) {
            continue;
        }

        if(zeal_host_key_up(keyCode)) {
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
    i2c_reset(&machine->i2c_bus, &machine->pio);
    device_reset(DEVICE(&machine->keyboard));
    device_reset(DEVICE(&machine->zvb));

    /* If a user program was specified, re-read the ROM and re-inject it so that
     * recompiled programs are picked up automatically on reset */
    if (config.arguments.uprog_filename != NULL) {
        flash_load_from_file(&machine->rom, config.images.rom, config.arguments.uprog_filename);
    }

#if CONFIG_ENABLE_DEBUGGER
    if(machine->dbg_enabled) {
        machine->dbg_state = ST_PAUSED;
    }
#endif // CONFIG_ENABLE_DEBUGGER
    return 0;
}

#if CONFIG_ENABLE_DEBUGGER
#if CONFIG_FLTK_UI
/**
 * @brief Create the debugger shell if it does not exist yet.
 *
 * The FLTK build runs a single window: the shell is both the debugger and the
 * emulator display, and it stays on screen whether or not the debugger is on.
 */
static int zeal_shell_ensure(zeal_t* machine)
{
    if (machine->dbg_ui != NULL) {
        return 0;
    }
    dbg_ui_init_args_t args;
    debugger_host_frontend_args(&machine->dbg, &args);
    if (debugger_ui_init(&machine->dbg_ui, &args) != 0) {
        return -1;
    }
    /* The shell replaces the host window rather than sitting next to it. */
    zeal_host_show(false);
    machine->dbg_frontend_visible = true;
    machine->dbg_last_frame = zeal_host_time();
    debugger_ui_show(machine->dbg_ui, true);
    return 0;
}
#endif // CONFIG_FLTK_UI
#endif // CONFIG_ENABLE_DEBUGGER

int zeal_init(zeal_t* machine)
{
    int err = 0;
    if (machine == NULL) {
        return 1;
    }

    memset(machine, 0, sizeof(*machine));
    vtimer_init();
    machine->headless = config.arguments.headless;
    /* Running headless with an explicitly requested debugger means the console
     * debugger front-end: the console is the only headless debugger UI. */
    if (machine->headless && !config.arguments.console &&
        config.debugger.enabled == DEBUGGER_STATE_ARG) {
        config.arguments.console = true;
    }
#if CONFIG_ENABLE_DEBUGGER
    machine->dbg_read_memory = debug_read_memory;
    machine->dbg.running = true;
    /* Set the debug mode in the machine structure as soon as possible.
     * In console mode the debugger is only enabled when explicitly requested
     * with --debug; the config file must not implicitly enable it. */
    if (config.arguments.console) {
        machine->dbg_enabled = (config.debugger.enabled == DEBUGGER_STATE_ARG);
    } else {
        machine->dbg_enabled = config_debugger_enabled() && !machine->headless;
    }
#endif // CONFIG_ENABLE_DEBUGGER

    if (!machine->headless) {
        /* Initialize the UI. It must be done before any shader is created! */
        const zeal_host_config_t host_config = {
            .width = 640,
            .height = 480,
            .title = WIN_NAME,
            .resizable = true,
        };
        if (!zeal_host_init(&host_config)) {
            return 1;
        }
        zeal_host_frame_rate(60);
        notif_reset();

        /* Draw one frame so the host can attach its peripherals (ie; gamepads) */
        zeal_host_frame_begin();
        zeal_host_clear((zeal_host_color_t){0, 0, 0, 255});
        zeal_host_frame_end();

#if CONFIG_ENABLE_DEBUGGER
        config_window_set(machine->dbg_enabled);
#endif // CONFIG_ENABLE_DEBUGGER
    }

#if CONFIG_ENABLE_DEBUGGER
    /* The debugger back-end is GUI-free and also drives the headless console
     * debugger, so initialize it regardless of the rendering mode. */

    /* Load symbols if provided */
    if (config.arguments.map_file) {
        debugger_load_symbols(&machine->dbg, config.arguments.map_file);
    }
    if (config.arguments.breakpoints) {
        debugger_set_breakpoints_str(&machine->dbg, config.arguments.breakpoints);
    }
#endif // CONFIG_ENABLE_DEBUGGER

    z80_init(&machine->cpu);
#if CONFIG_ENABLE_DEBUGGER
    zeal_debugger_init(machine, &machine->dbg);
    debugger_bind(&machine->dbg);
#endif
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

    err = at24c512_init(&machine->eeprom, config.images.eeprom);
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
        zeal_audio_set_volume(config.audio.volume / 100.0f);
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
#if CONFIG_FLTK_UI
    else if (!machine->headless) {
        /* The shell is the desktop's only window, so it comes up with the debug panels
         * hidden even when the emulator starts without the debugger. */
        if (zeal_shell_ensure(machine) != 0) return -1;
        debugger_ui_set_debugging(machine->dbg_ui, false);
    }
#endif // CONFIG_FLTK_UI
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
    if (machine->dbg_state != ST_RUNNING) {
        return;
    }
    /* Skip if a UI shortcut consumed the input */
    if (zeal_ui_input(machine)) {
        return;
    }
    if (machine->dbg_frontend_visible) return;

    /* A retained but hidden FLTK window must not suppress guest input. */
#endif
    zeal_read_keyboard(machine, HOST_KEYB_CHECK_PERIOD);
}


/**
 * @brief Execute a single Z80 instruction and advance the virtual timer.
 * If --no-reset is set and the PC returns to 0, the machine is flagged to exit.
 */
static void zeal_step(zeal_t* machine)
{
    const int elapsed_tstates = z80_step(&machine->cpu);
    if (config.arguments.no_reset && machine->cpu.pc == 0) {
        /* PC is back to 0, that's a software reset! */
        log_printf("[ZEAL] PC returned to 0x0000 after running (cyc=%lu), exiting\n", machine->cpu.cyc);
        zeal_exit(machine);
        return;
    }

    vtimer_tick(elapsed_tstates);
}


/**
 * @brief Put the rendered frame on screen.
 *
 * The software blitter keeps its frame in CPU memory, so hand that to the host
 * directly. A GPU blitter has no such buffer and is drawn from its texture instead;
 * only builds that keep Raylib compile that branch.
 */
static void zeal_present_frame(zeal_t* machine, float x, float y, float w, float h)
{
    int width = 0, height = 0, pitch = 0;
    bool rgb565 = false;
    const void* pixels = zvb_output_pixels(&machine->zvb, &width, &height, &pitch, &rgb565);
    if (pixels != NULL) {
        zeal_host_present(pixels, width, height, pitch,
                          rgb565 ? ZEAL_HOST_RGB565 : ZEAL_HOST_RGBA8888,
                          (zeal_host_rect_t){x, y, w, h});
        return;
    }
#if ZVB_BLITTER_SHADER
    DrawTexturePro(zvb_output_texture(&machine->zvb),
                   (Rectangle){0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT},
                   (Rectangle){x, y, w, h}, (Vector2){0, 0}, 0.0f, WHITE);
#endif
}

#if CONFIG_ENABLE_DEBUGGER
int zeal_debug_enable(zeal_t* machine)
{
    if (machine->headless) {
        /* Headless console debugger: no window to configure */
        machine->dbg_enabled = true;
        machine->dbg_state = ST_PAUSED;
        return config.arguments.console ? 0 : -1;
    }

    if (!machine->dbg_enabled) config_window_update(false);
    machine->dbg_enabled = true;
    machine->dbg_state = ST_PAUSED;
#if CONFIG_FLTK_UI
    if (zeal_shell_ensure(machine) != 0) return -1;
    zeal_host_frame_rate(0);
    debugger_ui_set_debugging(machine->dbg_ui, true);
#endif
    return 0;
}

int zeal_debug_disable(zeal_t* machine)
{
    machine->dbg_enabled = false;
    machine->dbg_state = ST_RUNNING;
#if CONFIG_FLTK_UI
    /* The shell keeps the window; only the debug panels go away, so the video fills
     * it while the emulator runs. */
    if (machine->dbg_ui != NULL) {
        debugger_ui_set_debugging(machine->dbg_ui, false);
    }
    zeal_host_frame_rate(60);
#endif
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
    const double profile_start = zeal_host_time();
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
        if (machine->dbg_frontend_visible && zeal_host_time() - machine->dbg_last_frame < 1.0/30) return 1;
        zvb_force_render(&machine->zvb);
    } else {
        /* Do not proceed, the CPU is currently running and the ZVB doens't need to be refreshed yet */
        return 0;
    }

    if (machine->dbg_ui && machine->dbg_frontend_visible) {
        double remaining = 1.0/60 - (zeal_host_time() - machine->dbg_last_frame);
        if (remaining > 0) zeal_host_wait(remaining);
        machine->dbg_last_frame = zeal_host_time();
        debugger_capture_video(&machine->dbg);
        debugger_ui_refresh(machine->dbg_ui);
        zeal_host_poll();
    } else {
        zeal_host_frame_begin();
        zeal_host_clear((zeal_host_color_t){0, 0, 0, 255});
        float scale = fminf((float)zeal_host_width()/ZVB_MAX_RES_WIDTH,(float)zeal_host_height()/ZVB_MAX_RES_HEIGHT);
        float width = ZVB_MAX_RES_WIDTH*scale, height=ZVB_MAX_RES_HEIGHT*scale;
        zeal_present_frame(machine, (zeal_host_width()-width)/2, (zeal_host_height()-height)/2,
                           width, height);
        notif_render(zeal_host_width()-notif_estimate_width()-20,10);
        if(show_fps) zeal_host_fps(10,10);
        zeal_host_frame_end();
    }

#if CONFIG_PROFILE_RENDER
    zvb_profile_frame(zeal_host_time() - profile_start);
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
            uint8_t opcode=machine->dbg_read_memory(machine,machine->cpu.pc);
            bool call=opcode==0xcd || (opcode&0xc7)==0xc4 || (opcode&0xc7)==0xc7;
            if (call) debugger_set_temporary_breakpoint(&machine->dbg,(uint16_t)(machine->cpu.pc+instr_size));
            machine->dbg_state=call?ST_RUNNING:ST_REQ_STEP;
        }

        const int elapsed_tstates = z80_step(&machine->cpu);
        if (config.arguments.no_reset && machine->cpu.pc == 0) machine->should_exit=true;

        vtimer_tick(elapsed_tstates);

        /* Check if we reached a breakpoint or if we have to do a single step */
        if (machine->dbg_state != ST_PAUSED && (machine->dbg_state == ST_REQ_STEP ||
            debugger_is_breakpoint_set(&machine->dbg, machine->cpu.pc)))
        {
            debugger_record(&machine->dbg, machine->dbg_state == ST_REQ_STEP ? DBG_REASON_STEP : DBG_REASON_BREAKPOINT,
                machine->cpu.pc, 0);
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

/**
 * @brief Run the machine while the debugger state machine allows it, up to
 * max_tstates T-states (0 = until the debugger pauses). Used by the headless
 * console debugger.
 *
 * Handles step and step-over requests, ticking the vtimer after each
 * instruction, and stops as soon as a breakpoint is hit or the budget is
 * spent. The machine is always left ST_PAUSED.
 *
 * @return true if stopped because of a breakpoint or step request, false if
 *         the budget was spent or the machine was flagged to exit.
 */
bool zeal_debugger_run(zeal_t* machine, unsigned long max_tstates)
{
    const unsigned long target = machine->cpu.cyc + max_tstates;
    bool debugger_stop = false;

    while (!machine->should_exit) {
        /* A step-over request sets a temporary breakpoint past the call */
        if (machine->dbg_state == ST_REQ_STEP_OVER) {
            const int instr_size = z80_instruction_size(&machine->cpu);
            uint8_t opcode=machine->dbg_read_memory(machine,machine->cpu.pc);
            bool call=opcode==0xcd || (opcode&0xc7)==0xc4 || (opcode&0xc7)==0xc7;
            if (call) debugger_set_temporary_breakpoint(&machine->dbg,(uint16_t)(machine->cpu.pc+instr_size));
            machine->dbg_state=call?ST_RUNNING:ST_REQ_STEP;
        }

        const int elapsed = z80_step(&machine->cpu);
        vtimer_tick(elapsed);

        /* Stop on a single step or a breakpoint */
        if (machine->dbg_state != ST_PAUSED && (machine->dbg_state == ST_REQ_STEP ||
            debugger_is_breakpoint_set(&machine->dbg, machine->cpu.pc))) {
            debugger_record(&machine->dbg, machine->dbg_state == ST_REQ_STEP ? DBG_REASON_STEP : DBG_REASON_BREAKPOINT,
                machine->cpu.pc, 0);
            machine->dbg_state = ST_PAUSED;
            debugger_clear_breakpoint_if_temporary(&machine->dbg, machine->cpu.pc);
            debugger_stop = true;
            break;
        }

        if (machine->dbg_state == ST_PAUSED) { debugger_stop=true; break; }

        /* Stop once the requested number of T-states has been spent */
        if (max_tstates > 0 && machine->cpu.cyc >= target) {
            break;
        }
    }

    machine->dbg_state = ST_PAUSED;
    return debugger_stop;
}
#endif // CONFIG_ENABLE_DEBUGGER


/**
 * @brief Run Zeal 8-bit Computer VM in normal mode.
 *
 * Returns 1 if the screen was rendered, 0 else
 */
#if !CONFIG_ENABLE_DEBUGGER
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
        const double profile_start = zeal_host_time();
#endif
        const int screen_w = zeal_host_width();
        const int screen_h = zeal_host_height();
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

        zeal_host_frame_begin();
            zeal_host_clear((zeal_host_color_t){80, 80, 80, 255});
            zeal_present_frame(machine, pos_x, pos_y, draw_w, draw_h);
            /* Show notifications on the top-right of the visible content */
            notif_render(pos_x + draw_w - notif_estimate_width() - 20, pos_y + 10);
            if(show_fps == true) {
                zeal_host_fps(10, 10);
            }
        zeal_host_frame_end();

#if CONFIG_PROFILE_RENDER
        zvb_profile_frame(zeal_host_time() - profile_start);
#endif
    }
    return rendered;
}

#endif

static void zeal_loop(zeal_t* machine)
{
    int rendered = 0;
    unsigned slice = 0;
    /**
     * When compiling for WASM, it is not necessary to poll for a close request as often as possible.
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

        if (machine->should_exit || ++slice >= 16384) {
#if PLATFORM_WEB
            emscripten_sleep(0);
#else
            zeal_host_poll();
#endif
            break;
        }
        int frame_rendered = 0;
#if CONFIG_ENABLE_DEBUGGER
        frame_rendered = zeal_dbg_mode_run(machine);
#else
        frame_rendered = zeal_normal_mode_run(machine);
#endif
        rendered += frame_rendered;
        if (frame_rendered > 0) {
            snes_adapter_update(&machine->snes_adapter);
        }
    }
}

/**
 * @brief Run the machine in headless mode (no window/rendering).
 *
 * In console mode it is driven by commands from stdin (console_run()).
 * Otherwise it auto-runs until it exits or the requested number of
 * T-states is reached.
 */
static void zeal_run_headless(zeal_t* machine)
{
    if (config.arguments.console) {
        console_run(machine);
    } else {
        while (!machine->should_exit) {
            zeal_step(machine);
            if (config.arguments.headless_run_ticks > 0 && machine->cpu.cyc >= config.arguments.headless_run_ticks) {
                log_printf("[ZEAL] Ran for %lu ticks\n", machine->cpu.cyc);
                break;
            }
        }
    }

#if CONFIG_ENABLE_DEBUGGER
    debugger_unbind(&machine->dbg);
    debugger_deinit(&machine->dbg);
#endif
    snes_adapter_detach(&machine->snes_adapter);
    zvb_deinit(&machine->zvb);
}

/**
 * @brief Run the machine for a given number of Z80 T-states.
 * Used by the console 'wait' command to make test scripts deterministic.
 */
void zeal_run_for_tstates(zeal_t* machine, unsigned long tstates)
{
#if CONFIG_ENABLE_DEBUGGER
    if (machine->dbg_enabled) {
        /* Debugger mode: honor breakpoints and step requests while running */
        debugger_continue(&machine->dbg);
        zeal_debugger_run(machine, tstates);
        return;
    }
#endif
    const unsigned long target = machine->cpu.cyc + tstates;

    while (machine->cpu.cyc < target && !machine->should_exit) {
        zeal_step(machine);
    }
}

void zeal_exit(zeal_t* machine)
{
    machine->should_exit = true;
}

/**
 * @brief One iteration of the host loop: service the debugger UI, then run a frame.
 *
 * @return false when the emulator wants the loop to stop.
 */
static bool zeal_tick(zeal_t* machine)
{
    if (machine->should_exit) {
        return false;
    }
#if CONFIG_ENABLE_DEBUGGER
    if (!machine->dbg.running) {
        return false;
    }
    debugger_ui_poll(machine->dbg_ui);
    if (machine->should_exit) {
        return false;
    }
#endif // CONFIG_ENABLE_DEBUGGER
    zeal_loop(machine);
    return !machine->should_exit;
}

static bool zeal_tick_cb(void* user)
{
    return zeal_tick((zeal_t*)user);
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

    /* The host owns the main loop: FLTK drives it from its idle callback on desktop,
     * Raylib from its polling loop on the web. */
    zeal_host_run(zeal_tick_cb, machine);

#if CONFIG_ENABLE_DEBUGGER
    config_window_update(machine->dbg_enabled);

    if(machine->dbg_ui != NULL) {
        debugger_ui_deinit(machine->dbg_ui);
    }
#else
        config_window_update(false);
#endif // CONFIG_ENABLE_DEBUGGER

#if CONFIG_ENABLE_DEBUGGER
    debugger_unbind(&machine->dbg);
    debugger_deinit(&machine->dbg);
#endif
    snes_adapter_detach(&machine->snes_adapter);
    zvb_deinit(&machine->zvb);
    zeal_host_shutdown();

    return ret;
}
