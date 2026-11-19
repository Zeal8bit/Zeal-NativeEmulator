/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <emscripten.h>

#include "frontend/wasm/frontend.h"
#include "frontend/wasm/snes_adapter.h"
#include "frontend/wasm/keys.h"
#include "frontend/wasm/video.h"
#include "frontend/wasm/audio.h"
#include "frontend/common/app.h"
#include "utils/log.h"
#include "utils/config.h"
#include "utils/notif.h"
#include "utils/vtimer.h"

#define RAYLIB_KEY_COUNT    384

/**
 * @brief Period, in T-states, between host keyboard polls (15ms).
 */
#define HOST_KEYB_CHECK_PERIOD   US_TO_TSTATES(15000)

typedef enum {
    KEY_NOT_PRESSED,
    KEY_PRESSED,
    KEY_REPEATED,
} kb_key_state_t;

typedef struct {
    kb_key_state_t state;
    int duration;
} kb_keys_t;

static void host_keyboard_check_cb(void* userdata);
static vtimer_node_t host_keyb_timer;
static bool window_opened;

/**
 * @brief Array used to key tracked of the key states on the host. This array will help simulate
 * key press, release and repeat.
 */
static kb_keys_t RAYLIB_KEYS[RAYLIB_KEY_COUNT];

EMSCRIPTEN_KEEPALIVE volatile
#if CONFIG_SHOW_FPS
    bool show_fps = true;
#else
    bool show_fps = false;
#endif

static int key_can_repeat(int code)
{
    // clang-format off
    const int modifiers[] = {
        KEY_LEFT_SHIFT,  KEY_LEFT_CONTROL,  KEY_LEFT_ALT,  KEY_LEFT_SUPER,
        KEY_RIGHT_SHIFT, KEY_RIGHT_CONTROL, KEY_RIGHT_ALT, KEY_RIGHT_SUPER,
        KEY_CAPS_LOCK,   KEY_NUM_LOCK
    };
    // clang-format on

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
    while (GetKeyPressed()) {}

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
    while ((keyCode = GetKeyPressed())) {
        if (keyCode < 0 || keyCode >= RAYLIB_KEY_COUNT) continue;
        RAYLIB_KEYS[keyCode].state = KEY_PRESSED;
        RAYLIB_KEYS[keyCode].duration = 0;
        key_pressed(&machine->keyboard, wasm_key(keyCode));
    }

    // look for newly released keys
    for (keyCode = 0; keyCode < RAYLIB_KEY_COUNT; keyCode++) {
        kb_keys_t* key = &RAYLIB_KEYS[keyCode];

        if (key->state == KEY_NOT_PRESSED) {
            continue;
        }

        if (IsKeyUp(keyCode)) {
            key->state = KEY_NOT_PRESSED;
            /* No need to clear the duration, it's done when the key is pressed */
            key_released(&machine->keyboard, wasm_key(keyCode));
            continue;
        }

        /* Key is still pressed, add the current delta to its duration and check it */
        key->duration += delta;

        if (key->state == KEY_PRESSED && key_can_repeat(keyCode) && key->duration >= start_delay) {
            key_pressed(&machine->keyboard, wasm_key(keyCode));
            key->state = KEY_REPEATED;
            key->duration = 0;
        } else if (key->state == KEY_REPEATED && key->duration >= repeat_delay) {
            key->duration = 0;
            key_pressed(&machine->keyboard, wasm_key(keyCode));
        }
    }
}

/**
 * @brief Periodic vtimer callback to poll the host keyboard for new key events.
 */
static void host_keyboard_check_cb(void* userdata)
{
    zeal_t* machine = (zeal_t*) userdata;

    /* Reschedule for the next periodic check */
    vtimer_schedule_tstates(&host_keyb_timer, HOST_KEYB_CHECK_PERIOD);

    zeal_read_keyboard(machine, HOST_KEYB_CHECK_PERIOD);
}

/**
 * @brief Execute a single Z80 instruction and advance the virtual timer.
 * If --no-reset is set and the PC returns to 0, the machine is flagged to exit.
 */

/**
 * @brief Run Zeal 8-bit Computer VM in normal mode.
 *
 * Returns 1 if the screen was rendered, 0 else
 */
static int zeal_normal_mode_run(zeal_t* machine)
{
    int rendered = 0;

    zeal_step_result_t result = zeal_step(machine, false);
    if (result & (ZEAL_STEP_FATAL | ZEAL_STEP_STOP)) return 2;
    if (result & ZEAL_STEP_FRAME) {
        rendered = 1;
#if CONFIG_PROFILE_RENDER
        const double profile_start = GetTime();
#endif
        const int screen_w = GetScreenWidth();
        const int screen_h = GetScreenHeight();
        const float texture_ratio = (float) ZVB_MAX_RES_WIDTH / ZVB_MAX_RES_HEIGHT;
        const float screen_ratio  = (float) screen_w / screen_h;

        int pos_x = 0;
        int pos_y = 0;

        int draw_w = ZVB_MAX_RES_WIDTH;
        int draw_h = ZVB_MAX_RES_HEIGHT;

        if (texture_ratio > screen_ratio) {
            /* Texture is "wider" than the screen, add bars on top/bottom */
            draw_w = screen_w;
            draw_h = (int) (screen_w / texture_ratio);
            pos_y = (screen_h - draw_h) / 2;
        } else {
            /* Texture is "taller" than the screen, add bars on left/right */
            draw_h = screen_h;
            draw_w = (int) (screen_h * texture_ratio);
            pos_x = (screen_w - draw_w) / 2;
        }

        Texture output = wasm_video_output(&machine->zvb);
        BeginDrawing();
        ClearBackground(DARKGRAY);
        DrawTexturePro(output,
                       (Rectangle){ 0, 0, ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT },
                       (Rectangle){ pos_x, pos_y, draw_w, draw_h },
                       (Vector2){ 0, 0 },
                       0.0f,
                       WHITE);
        /* Show notifications on the top-right of the visible content */
        notif_render(pos_x + draw_w - notif_estimate_width() - 20, pos_y + 10);
        if (show_fps == true) {
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
    /* Calling WindowShouldClose too often slows browser emulation. Yield through
     * Raylib/Asyncify and service browser work every two rendered frames. */
    while (rendered < 2) {

        const int frame_rendered = zeal_normal_mode_run(machine);
        rendered += frame_rendered;
        if (frame_rendered > 0) {
            snes_adapter_update(&frontend_snes);
        }
    }
}

int frontend_run(zeal_t* machine)
{
    if (machine == NULL) {
        return -1;
    }

    while (!machine->should_exit && !WindowShouldClose()) {
        zeal_service_web();
        if (machine->should_exit) break;
        zeal_loop(machine);
    }

    frontend_close(machine);

    return machine->compactflash.failed ? 1 : 0;
}

void frontend_reset_input(zeal_t* machine)
{
    zeal_read_keyboard_reset(machine);
}

void frontend_open(void)
{
    SetTraceLogLevel(WIN_LOG_LEVEL);
    /* Initialize the Raylib window. */
    InitWindow(640, 480, WIN_NAME);
    window_opened = true;
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    notif_reset();
    /* Force rendering the window so Raylib peripherals (e.g. gamepads) attach. */
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
}

void frontend_bind_input(zeal_t* machine)
{
    vtimer_init_node(&host_keyb_timer, &machine->cpu.clock, host_keyboard_check_cb, machine);
    vtimer_schedule_tstates(&host_keyb_timer, HOST_KEYB_CHECK_PERIOD);
    wasm_snes_init(&frontend_snes, &machine->snes_adapter);
}

int frontend_bind_output(zeal_t* machine)
{
    wasm_audio_init(&machine->zvb.sound, true);
    wasm_video_init(&machine->zvb);
    SetMasterVolume(config.audio.volume / 100.0f);
    if (config.audio.volume != 100) notif_show("Volume: %d%%", config.audio.volume);
    return 0;
}

void frontend_close(zeal_t* machine)
{
    vtimer_cancel(&host_keyb_timer);
    if (frontend_snes.core) {
        snes_adapter_detach(&frontend_snes);
        frontend_snes.core = NULL;
    }
    wasm_audio_deinit(&machine->zvb.sound);
    wasm_video_deinit();
#if CONFIG_PROFILE_RENDER
    machine->zvb.profile_time = NULL;
    machine->zvb.profile_render = NULL;
#endif
    if (window_opened) {
        CloseWindow();
        window_opened = false;
    }
}
