/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <emscripten.h>
#include "frontend/common/app.h"
#include "frontend.h"
#include "keys.h"
#include "controller.h"
#include "mouse.h"
#include "audio.h"
#include "video.h"
#if CONFIG_ENABLE_DEBUGGER
#include "web_debug.h"
#endif
#include "utils/log.h"

EMSCRIPTEN_KEEPALIVE volatile bool show_fps =
#if CONFIG_SHOW_FPS
    true;
#else
    false;
#endif

static void poll_input(zeal_t* machine)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            zeal_key_t key = wasm_key(event.key.scancode);
            if (key == ZEAL_KEY_NULL) {
                break;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                key_pressed(&machine->keyboard, key);
            } else {
                key_released(&machine->keyboard, key);
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            snes_controller_event(&event);
            break;
        default:
            snes_mouse_event(&frontend_snes.mouse, &event);
            break;
        }
    }
    snes_adapter_update(&frontend_snes);
}

zeal_step_result_t wasm_frame(zeal_t* machine)
{
    poll_input(machine);
    zeal_step_result_t result = ZEAL_STEP_NONE;
    bool debug = false;
#if CONFIG_ENABLE_DEBUGGER
    debug = machine->dbg_enabled;
    if (debug && web_debug_service(machine)) {
        zvb_force_render(&machine->zvb);
        result = ZEAL_STEP_FRAME;
    }
    if (!debug || machine->dbg_state != ST_PAUSED)
#endif
    {
        const uint64_t start = machine->cpu.clock.now_ns;
        do {
            result = zeal_step(machine, debug);
        } while (!(result & (ZEAL_STEP_FRAME | ZEAL_STEP_FATAL |
                            ZEAL_STEP_STOP | ZEAL_STEP_DEBUG_STOP)) &&
                 (!debug || machine->cpu.clock.now_ns - start < 20000000));
    }
#if CONFIG_ENABLE_DEBUGGER
    if (result & ZEAL_STEP_DEBUG_STOP) {
        web_debug_after_step(machine);
    }
    if (debug && (result & ZEAL_STEP_FRAME)) {
        web_debug_on_frame(machine);
    }
#endif
    return result;
}

bool wasm_present(zeal_t* machine)
{
    if (wasm_video_draw(&machine->zvb, show_fps)) return true;
    log_err_printf("[SDL] Render failed: %s\n", SDL_GetError());
    return false;
}

void frontend_reset_input(zeal_t* machine)
{
    (void) machine;
    SDL_ResetKeyboard();
}

void frontend_open(void)
{
}

void frontend_bind_input(zeal_t* machine)
{
    wasm_snes_init(&frontend_snes, &machine->snes_adapter);
}

int frontend_bind_output(zeal_t* machine)
{
    if (!SDL_Init(SDL_INIT_VIDEO) ||
        !wasm_video_init(&machine->zvb)) {
        log_err_printf("[SDL] Initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        log_err_printf("[SNES] Gamepads unavailable: %s\n", SDL_GetError());
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        wasm_audio_init(&machine->zvb.sound);
    } else {
        log_err_printf("[AUDIO] Playback unavailable: %s\n", SDL_GetError());
    }
    return 0;
}

void frontend_close(zeal_t* machine)
{
#if CONFIG_ENABLE_DEBUGGER
    web_debug_close();
#endif
    if (frontend_snes.core) {
        snes_adapter_detach(&frontend_snes);
        frontend_snes.core = NULL;
    }
    snes_controller_shutdown();
    wasm_audio_deinit(&machine->zvb.sound);
    wasm_video_deinit();
    SDL_Quit();
}
