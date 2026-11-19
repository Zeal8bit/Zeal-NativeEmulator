/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Desktop application lifecycle and run loop for the SDL3 + FLTK frontend.
 * FLTK owns every window and input event; SDL3 runs headless (no video) and
 * only provides audio playback and gamepads. Emulation, FLTK callbacks and
 * SDL polling all happen on this single thread. */

#include <stdio.h>
#include <SDL3/SDL.h>

#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/config.h"
#include "frontend/sdl-fltk/debug.h"
#include "frontend/common/app.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/notif.h"
#include "utils/vtimer.h"

/* Rebase after a long host stall rather than rapidly replaying missed time. */
#define NATIVE_PACING_MAX_LAG_NS    (100000000ULL)
/* Redraw rate of a paused debugger */
#define PAUSED_FRAME_SECONDS        (1.0 / 60.0)

zeal_t* fltk_machine;
static bool s_sdl_ready;

void frontend_open(bool debug)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    /* Keep default SIGINT/SIGTERM behaviour; SDL would turn them into an
     * SDL_EVENT_QUIT that this frontend never reads. */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_APP_NAME, FZ_WIN_NAME);
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        s_sdl_ready = true;
    } else {
        log_err_printf("[SDL] Initialization failed: %s\n", SDL_GetError());
    }
    notif_reset();
    ui_open(debug);
}

void frontend_bind_input(zeal_t* machine)
{
    fltk_machine = machine;
    fltk_keyboard_bind(machine);
    fltk_debug_bind(machine);
    fltk_snes_init(&frontend_snes, &machine->snes_adapter);
}

int frontend_bind_output(zeal_t* machine)
{
    fltk_audio_init(&machine->zvb.sound);
    fz_volume_set(config.audio.volume);
    if (config.audio.volume != 100) notif_show("Volume: %d%%", config.audio.volume);
    fltk_profile_bind(&machine->zvb);
    /* Saved project stops must exist before the first guest instruction */
    ui_machine_bound();
    fltk_debug_start(machine);
    return 0;
}

void frontend_reset_input(zeal_t* machine)
{
    (void) machine;
    fltk_keyboard_reset();
}

/**
 * @brief Run the machine until it produces a frame, then present it.
 * Returns false once the machine requested to stop.
 */
static bool run_frame(zeal_t* machine)
{
    for (;;) {
        const bool debugging = fltk_debug_active(machine);
        zeal_step_result_t result = zeal_step(machine, debugging);
        if (result & (ZEAL_STEP_FATAL | ZEAL_STEP_STOP)) return false;
        if (!(result & ZEAL_STEP_FRAME)) continue;

        const double profile_start = fltk_profile_frame_begin();
        const zvb_frame_t frame = zvb_frame(&machine->zvb);
        ui_present(frame.pixels, frame.width, frame.height);
        if (debugging) {
            /* Breakpoints and single steps must show their final state;
             * otherwise the UI throttles refreshes of a running machine. */
            static bool s_last_paused;
            const bool paused = fltk_debug_paused(machine);
            ui_debug_refresh((result & ZEAL_STEP_DEBUG_STOP) || (paused && !s_last_paused));
            s_last_paused = paused;
        }
        snes_controller_poll();
        snes_adapter_update(&frontend_snes);
        fltk_profile_frame_end(profile_start);
        return true;
    }
}

int frontend_run(zeal_t* machine)
{
    if (machine == NULL) return -1;

    uint64_t host_epoch = SDL_GetTicksNS();
    uint64_t emulated_epoch = vtimer_now_ns(&machine->cpu.clock);

    while (!machine->should_exit && !ui_closed()) {
        if (!fltk_debug_keep_running(machine)) break;
        const bool was_paused = fltk_debug_paused(machine);
        if (!run_frame(machine)) break;

        const uint64_t emulated_now = vtimer_now_ns(&machine->cpu.clock);
        if (was_paused || fltk_debug_paused(machine) || emulated_now < emulated_epoch) {
            /* A paused debugger only redraws the UI at a steady rate. */
            ui_wait(PAUSED_FRAME_SECONDS);
            host_epoch = SDL_GetTicksNS();
            emulated_epoch = vtimer_now_ns(&machine->cpu.clock);
            continue;
        }

        /* Match elapsed emulated time to wall time; keep servicing UI
         * events until the absolute deadline of this frame. */
        const uint64_t deadline = host_epoch + (emulated_now - emulated_epoch);
        uint64_t now = SDL_GetTicksNS();
        if (now > deadline + NATIVE_PACING_MAX_LAG_NS) {
            host_epoch = now;
            emulated_epoch = emulated_now;
            ui_wait(0);
            continue;
        }
        do {
            ui_wait(now < deadline ? (deadline - now) / 1e9 : 0);
            now = SDL_GetTicksNS();
        } while (now < deadline && !ui_closed() && !machine->should_exit);
    }

    config_window_update(fltk_debug_active(machine));
    frontend_close(machine);
    if (machine->compactflash.failed) return 1;
    config_save();
    return 0;
}

void frontend_close(zeal_t* machine)
{
    fltk_keyboard_unbind();
    fltk_debug_unbind(machine);
    if (frontend_snes.core) {
        snes_adapter_detach(&frontend_snes);
        frontend_snes.core = NULL;
    }
    fltk_audio_deinit(&machine->zvb.sound);
    fltk_profile_unbind(&machine->zvb);
    ui_close();
    snes_controller_shutdown();
    if (s_sdl_ready) {
        SDL_Quit();
        s_sdl_ready = false;
    }
    fltk_machine = NULL;
}
