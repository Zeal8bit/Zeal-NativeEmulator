/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio.h"
#include "../common/audio.h"

void wasm_audio_init(zvb_sound_t* sound)
{
    zvb_audio_init(sound);
}

void wasm_audio_deinit(zvb_sound_t* sound)
{
    zvb_audio_deinit(sound);
}
