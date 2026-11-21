/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Render profiling hooks when built without -Dprofile_render=true. */

#include "frontend/sdl-fltk/frontend.h"

void fltk_profile_bind(zvb_t* zvb)
{
    (void) zvb;
}

void fltk_profile_unbind(zvb_t* zvb)
{
    (void) zvb;
}

double fltk_profile_frame_begin(void)
{
    return 0.0;
}

void fltk_profile_frame_end(double start)
{
    (void) start;
}
