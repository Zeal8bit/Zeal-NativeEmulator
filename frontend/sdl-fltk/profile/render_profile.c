/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Optional render profiling (-Dprofile_render=true). */

#include <string.h>
#include <SDL3/SDL.h>
#include "frontend/sdl-fltk/frontend.h"
#include "utils/config.h"
#include "utils/log.h"

typedef struct {
    double window_start;
    double frame_total;
    double frame_max;
    double zvb_total;
    double zvb_max;
    uint64_t frames;
    uint64_t zvb_calls;
} render_profile_t;

static render_profile_t s_render_profile;
static double last_core_seconds;

static void fltk_profile_frame(double elapsed_seconds);

static double fltk_seconds(void)
{
    return SDL_GetTicksNS() / 1e9;
}

static double profile_time(void* userdata)
{
    (void) userdata;
    return fltk_seconds();
}

static void profile_render(void* userdata, double start)
{
    (void) userdata;
    const double elapsed = fltk_seconds() - start;
    last_core_seconds = elapsed;
    s_render_profile.zvb_total += elapsed;
    if (elapsed > s_render_profile.zvb_max) s_render_profile.zvb_max = elapsed;
    s_render_profile.zvb_calls++;
}

void fltk_profile_bind(zvb_t* zvb)
{
    zvb->profile_time = profile_time;
    zvb->profile_render = profile_render;
}

void fltk_profile_unbind(zvb_t* zvb)
{
    zvb->profile_time = NULL;
    zvb->profile_render = NULL;
}

double fltk_profile_frame_begin(void)
{
    return fltk_seconds();
}

void fltk_profile_frame_end(double start)
{
    fltk_profile_frame(fltk_seconds() - start);
}

static void fltk_profile_frame(double elapsed_seconds)
{
    if (!config.arguments.profile) {
        return;
    }

    const double now = fltk_seconds();
    if (s_render_profile.window_start == 0.0) {
        s_render_profile.window_start = now;
    }
    elapsed_seconds += last_core_seconds;
    s_render_profile.frame_total += elapsed_seconds;
    if (elapsed_seconds > s_render_profile.frame_max) {
        s_render_profile.frame_max = elapsed_seconds;
    }
    s_render_profile.frames++;

    if (now - s_render_profile.window_start < 1.0) {
        return;
    }

    const double frame_avg = s_render_profile.frames > 0
        ? s_render_profile.frame_total / s_render_profile.frames : 0.0;
    const double zvb_avg = s_render_profile.zvb_calls > 0
        ? s_render_profile.zvb_total / s_render_profile.zvb_calls : 0.0;
    log_printf(
        "[PROFILE][ZVB] frames=%llu frame=%.2f/%.2fms zvb=%.2f/%.2fms\n",
        (unsigned long long)s_render_profile.frames,
        frame_avg * 1000.0,
        s_render_profile.frame_max * 1000.0,
        zvb_avg * 1000.0,
        s_render_profile.zvb_max * 1000.0);

    memset(&s_render_profile, 0, sizeof(s_render_profile));
    s_render_profile.window_start = now;
}
