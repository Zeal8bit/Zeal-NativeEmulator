/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "frontend/nuklear/frontend.h"
#include "frontend/nuklear/snes_adapter.h"
#include "frontend/nuklear/keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hw/zeal.h"
#if CONFIG_ENABLE_DEBUGGER
#include "frontend/common/symbols.h"
#endif
#include "frontend/common/media.h"
#include "frontend/common/host.h"
#include "frontend/common/hostfs.h"
#include "frontend/nuklear/video.h"
#include "frontend/nuklear/audio.h"
#include "frontend/common/app.h"
#include "frontend/nuklear/frontend.h"
#include "utils/log.h"
#include "utils/config.h"
#include "utils/notif.h"
#include "utils/vtimer.h"
#ifdef PLATFORM_WEB
#include <emscripten.h>
#endif

#define RAYLIB_KEY_COUNT 384

/**
 * @brief Period, in T-states, between host keyboard polls (15ms).
 */
#define HOST_KEYB_CHECK_PERIOD US_TO_TSTATES(15000)

/* Rebase after a long host stall rather than rapidly replaying missed time. */
#define NATIVE_PACING_MAX_LAG_SECONDS (0.1)


typedef enum
{
    KEY_NOT_PRESSED,
    KEY_PRESSED,
    KEY_REPEATED,
} kb_key_state_t;

typedef struct
{
    kb_key_state_t state;
    int duration;
} kb_keys_t;

bool zeal_ui_input(zeal_t *machine);

static void host_keyboard_check_cb(void *userdata);
static vtimer_node_t host_keyb_timer;
static bool window_opened;

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


static int key_can_repeat(int code)
{
    const int modifiers[] = {
        KEY_LEFT_SHIFT, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_LEFT_SUPER,
        KEY_RIGHT_SHIFT, KEY_RIGHT_CONTROL, KEY_RIGHT_ALT, KEY_RIGHT_SUPER,
        KEY_CAPS_LOCK, KEY_NUM_LOCK};

    for (unsigned int i = 0; i < DIM(modifiers); i++)
    {
        if (modifiers[i] == code)
        {
            return false;
        }
    }
    return true;
}

static void zeal_read_keyboard_reset(zeal_t *machine)
{
    (void)machine;
    /* Clear Raylib's key states */
    while (GetKeyPressed())
    {
    }

    for (int i = 0; i < RAYLIB_KEY_COUNT; i++)
    {
        RAYLIB_KEYS[i].duration = 0;
        RAYLIB_KEYS[i].state = KEY_NOT_PRESSED;
    }
}

static void zeal_read_keyboard(zeal_t *machine, int delta)
{
    int keyCode;

    /* The initial delay is ~500ms before repeat starts */
    const int start_delay = us_to_tstates(500000);
    /* Then, repeat the key every 50ms */
    const int repeat_delay = us_to_tstates(50000);

    // look for newly pressed keys
    while ((keyCode = GetKeyPressed()))
    {
        if (keyCode < 0 || keyCode >= RAYLIB_KEY_COUNT) continue;
        RAYLIB_KEYS[keyCode].state = KEY_PRESSED;
        RAYLIB_KEYS[keyCode].duration = 0;
        key_pressed(&machine->keyboard, nuklear_key(keyCode));
    }

    // look for newly released keys
    for (keyCode = 0; keyCode < RAYLIB_KEY_COUNT; keyCode++)
    {
        kb_keys_t *key = &RAYLIB_KEYS[keyCode];

        if (key->state == KEY_NOT_PRESSED)
        {
            continue;
        }

        if (IsKeyUp(keyCode))
        {
            key->state = KEY_NOT_PRESSED;
            /* No need to clear the duration, it's done when the key is pressed */
            key_released(&machine->keyboard, nuklear_key(keyCode));
            continue;
        }

        /* Key is still pressed, add the current delta to its duration and check it */
        key->duration += delta;

        if (key->state == KEY_PRESSED && key_can_repeat(keyCode) && key->duration >= start_delay)
        {
            key_pressed(&machine->keyboard, nuklear_key(keyCode));
            key->state = KEY_REPEATED;
            key->duration = 0;
        }
        else if (key->state == KEY_REPEATED && key->duration >= repeat_delay)
        {
            key->duration = 0;
            key_pressed(&machine->keyboard, nuklear_key(keyCode));
        }
    }
}


/**
 * @brief Periodic vtimer callback to poll the host keyboard for new key events.
 */
static void host_keyboard_check_cb(void *userdata)
{
    zeal_t *machine = (zeal_t *)userdata;

    /* Reschedule for the next periodic check */
    vtimer_schedule_tstates(&host_keyb_timer, HOST_KEYB_CHECK_PERIOD);

#if CONFIG_ENABLE_DEBUGGER
    /* Skip polling if CPU is paused in debug mode */
    if (machine->dbg_enabled && machine->dbg_state != ST_RUNNING)
    {
        return;
    }
    /* Skip if a UI shortcut consumed the input */
    if (zeal_ui_input(machine))
    {
        return;
    }
    /* Skip if the debugger main view doesn't have focus */
    if (nuklear_ui != NULL && !debugger_ui_main_view_focused(nuklear_ui))
    {
        return;
    }
#endif
    zeal_read_keyboard(machine, HOST_KEYB_CHECK_PERIOD);
}

/**
 * @brief Execute a single Z80 instruction and advance the virtual timer.
 * If --no-reset is set and the PC returns to 0, the machine is flagged to exit.
 */


#if CONFIG_ENABLE_DEBUGGER
int zeal_debug_enable(zeal_t *machine)
{
    config_window_update(machine->dbg_enabled);
    int ret = 0;
    machine->dbg_enabled = true;
    machine->dbg_state = ST_PAUSED;
    config_window_set(true);
    if (nuklear_ui == NULL)
    {
        dbg_ui_init_args_t args = {
            .main_view = &nuklear_video.main_texture,
            .zvb = &machine->zvb,
        };
        args.debug_views = zvb_get_debug_textures(&machine->zvb, &args.debug_views_count);
        ret = debugger_ui_init(&nuklear_ui, &args);
    }
    return machine->compactflash.failed ? 1 : ret;
}

int zeal_debug_disable(zeal_t *machine)
{
    config_window_update(machine->dbg_enabled);
    machine->dbg_enabled = false;
    machine->dbg_state = ST_RUNNING;
    config_window_set(false);
    return 0;
}

void zeal_debug_toggle(dbg_t *dbg)
{
    if (dbg == NULL)
        return;
    zeal_t *machine = (zeal_t *)(dbg->arg);

    if (machine->dbg_enabled)
    {
        log_printf("[DEBUGGER]: Disabled\n");
        zeal_debug_disable(machine);
    }
    else
    {
        log_printf("[DEBUGGER]: Enabled\n");
        zeal_debug_enable(machine);
    }
}

/**
 * Returns 1 if rendered, 0 else
 */
static int zeal_dbg_mode_display(zeal_t *machine)
{
#if CONFIG_PROFILE_RENDER
    const double profile_start = GetTime();
#endif

    /**
     * Prepare the rendering, if the returned value is true, we can
     * proceed to rendering, else, we don't need to update the view.
     * However, if the CPU is paused (breakpoint/step), force the rendering.
     */
    nuklear_video_upload(&machine->zvb);
    /* Update only the VRAM debug view currently focused in the panel */
    const dbg_vram_t debug_view = debugger_ui_vram_panel_opened(nuklear_ui);
    if (debug_view != DBG_VIEW_NONE)
    {
        zvb_render_debug_textures(&machine->zvb, debug_view);
    }

    debugger_ui_prepare_render(nuklear_ui, &machine->dbg);
    BeginDrawing();
    /* Grey brackground */
    ClearBackground((Color){0x63, 0x63, 0x63, 0xff});
    debugger_ui_render(nuklear_ui, &machine->dbg);
    notif_render(GetScreenWidth() - notif_estimate_width() - 20, 10);
    if (show_fps == true)
    {
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
static int zeal_dbg_mode_run(zeal_t *machine)
{
    zeal_step_result_t result = zeal_step(machine, true);
    if (result & (ZEAL_STEP_FATAL | ZEAL_STEP_STOP)) return 2;
    if (!(result & ZEAL_STEP_FRAME)) return 0;

    int rendered = zeal_dbg_mode_display(machine);
    /* If the CPU is paused, we didn't check the UI input! Check it once per frame */
    if (rendered && machine->dbg_state == ST_PAUSED)
    {
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
static int zeal_normal_mode_run(zeal_t *machine)
{
    int rendered = 0;

    zeal_step_result_t result = zeal_step(machine, false);
    if (result & (ZEAL_STEP_FATAL | ZEAL_STEP_STOP)) return 2;
    if (result & ZEAL_STEP_FRAME)
    {
        rendered = 1;
#if CONFIG_PROFILE_RENDER
        const double profile_start = GetTime();
#endif
        const int screen_w = GetScreenWidth();
        const int screen_h = GetScreenHeight();
        const float texture_ratio = (float)ZVB_MAX_RES_WIDTH / ZVB_MAX_RES_HEIGHT;
        const float screen_ratio = (float)screen_w / screen_h;

        int pos_x = 0;
        int pos_y = 0;

        int draw_w = ZVB_MAX_RES_WIDTH;
        int draw_h = ZVB_MAX_RES_HEIGHT;

        if (texture_ratio > screen_ratio)
        {
            /* Texture is "wider" than the screen, add bars on top/bottom */
            draw_w = screen_w;
            draw_h = (int)(screen_w / texture_ratio);
            pos_y = (screen_h - draw_h) / 2;
        }
        else
        {
            /* Texture is "taller" than the screen, add bars on left/right */
            draw_h = screen_h;
            draw_w = (int)(screen_h * texture_ratio);
            pos_x = (screen_w - draw_w) / 2;
        }


        Texture output = nuklear_video_output(&machine->zvb);
        BeginDrawing();
        ClearBackground(DARKGRAY);
        DrawTexturePro(output,
                       (Rectangle){0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT},
                       (Rectangle){pos_x, pos_y, draw_w, draw_h},
                       (Vector2){0, 0},
                       0.0f,
                       WHITE);
        /* Show notifications on the top-right of the visible content */
        notif_render(pos_x + draw_w - notif_estimate_width() - 20, pos_y + 10);
        if (show_fps == true)
        {
            DrawFPS(10, 10);
        }
        EndDrawing();

#if CONFIG_PROFILE_RENDER
        zvb_profile_frame(GetTime() - profile_start);
#endif
    }
    return rendered;
}

static void zeal_loop(zeal_t *machine)
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
    while (rendered < 2)
    {
#else
    while (rendered < 1)
    {
#endif

        int frame_rendered = 0;
#if CONFIG_ENABLE_DEBUGGER
        if (machine->dbg_enabled)
        {
            frame_rendered = zeal_dbg_mode_run(machine);
        }
        else
#endif // CONFIG_ENABLE_DEBUGGER
        {
            frame_rendered = zeal_normal_mode_run(machine);
        }
        rendered += frame_rendered;
        if (frame_rendered > 0)
        {
            snes_adapter_update(&frontend_snes);
        }
    }
}

int frontend_run(zeal_t *machine)
{
    int ret = 0;

    if (machine == NULL)
    {
        return -1;
    }

#ifndef PLATFORM_WEB
    double host_epoch = GetTime();
    uint64_t emulated_epoch = vtimer_now_ns(&machine->cpu.clock);
#endif
    while (!machine->should_exit && !WindowShouldClose())
    {
#ifdef PLATFORM_WEB
        zeal_service_web();
        if (machine->should_exit) break;
#endif
#if CONFIG_ENABLE_DEBUGGER
        if (!machine->dbg.running)
            break;
#endif // CONFIG_ENABLE_DEBUGGER
#ifndef PLATFORM_WEB
        bool paused = false;
#if CONFIG_ENABLE_DEBUGGER
        paused = machine->dbg_enabled && machine->dbg_state == ST_PAUSED;
#endif
        /* Running emulation uses the absolute deadline below. Only a paused
         * debugger needs Raylib to limit UI redraws independently of the CPU. */
        SetTargetFPS(paused ? 60 : 0);
#endif
        zeal_loop(machine);
#ifndef PLATFORM_WEB
        const double now = GetTime();
        const uint64_t emulated_now = vtimer_now_ns(&machine->cpu.clock);
#if CONFIG_ENABLE_DEBUGGER
        paused = paused || (machine->dbg_enabled && machine->dbg_state == ST_PAUSED);
#endif
        if (paused || emulated_now < emulated_epoch)
        {
            host_epoch = now;
            emulated_epoch = emulated_now;
        }
        else
        {
            /* Match elapsed emulated time to wall time, including the exact
             * 16.768 ms video period. Sleep overshoot is repaid next frame. */
            const double remaining = host_epoch + (emulated_now - emulated_epoch) / 1e9 - now;
            if (remaining < -NATIVE_PACING_MAX_LAG_SECONDS)
            {
                host_epoch = now;
                emulated_epoch = emulated_now;
            }
            else if (remaining > 0)
            {
                WaitTime(remaining);
            }
        }
#endif
    }

#if CONFIG_ENABLE_DEBUGGER
    config_window_update(machine->dbg_enabled);
    dbg_ui_config_capture();

#else
    config_window_update(false);
#endif // CONFIG_ENABLE_DEBUGGER
    frontend_close(machine);

    return machine->compactflash.failed ? 1 : ret;
}

struct dbg_ui_t* nuklear_ui;

void frontend_reset_input(zeal_t* machine)
{
    zeal_read_keyboard_reset(machine);
}

void frontend_open(bool debug)
{
    SetTraceLogLevel(WIN_LOG_LEVEL);
#ifndef PLATFORM_WEB
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
    InitWindow(640, 480, WIN_NAME);
    window_opened = true;
    SetExitKey(KEY_NULL);
#ifndef PLATFORM_WEB
    SetWindowFocused();
#endif
    SetTargetFPS(60);
    notif_reset();
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
#if CONFIG_ENABLE_DEBUGGER
    config_window_set(debug);
#else
    (void)debug;
#endif
}

void frontend_bind_input(zeal_t* machine)
{
    vtimer_init_node(&host_keyb_timer, &machine->cpu.clock, host_keyboard_check_cb, machine);
    vtimer_schedule_tstates(&host_keyb_timer, HOST_KEYB_CHECK_PERIOD);
    nuklear_snes_init(&frontend_snes, &machine->snes_adapter);
}

int frontend_bind_output(zeal_t* machine)
{
    nuklear_audio_init(&machine->zvb.sound, true);
    nuklear_video_init(&machine->zvb);
    SetMasterVolume(config.audio.volume / 100.0f);
    if (config.audio.volume != 100) notif_show("Volume: %d%%", config.audio.volume);
#if CONFIG_ENABLE_DEBUGGER
    if (machine->dbg_enabled) {
        int err = zeal_debug_enable(machine);
        machine->dbg_state = ST_RUNNING;
        return err;
    }
#endif
    return 0;
}

void frontend_close(zeal_t* machine)
{
    vtimer_cancel(&host_keyb_timer);
#if CONFIG_ENABLE_DEBUGGER
    if (nuklear_ui) {
        debugger_ui_deinit(nuklear_ui);
        nuklear_ui = NULL;
    }
#endif
    if (frontend_snes.core) {
        snes_adapter_detach(&frontend_snes);
        frontend_snes.core = NULL;
    }
    nuklear_audio_deinit(&machine->zvb.sound);
    nuklear_video_deinit();
#if CONFIG_PROFILE_RENDER
    machine->zvb.profile_time = NULL;
    machine->zvb.profile_render = NULL;
#endif
    if (window_opened) {
        CloseWindow();
        window_opened = false;
    }
}
