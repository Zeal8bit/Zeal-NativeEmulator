/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "frontend/sdl-fltk/frontend.h"
#include "../../common/audio.h"

void fltk_audio_init(zvb_sound_t* sound)
{
    zvb_audio_init(sound);
}

void fltk_audio_deinit(zvb_sound_t* sound)
{
    zvb_audio_deinit(sound);
}

void fltk_audio_set_gain(float gain)
{
    zvb_audio_set_gain(gain);
}
