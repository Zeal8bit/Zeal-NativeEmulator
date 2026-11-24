/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "hw/zvb/zvb_sound.h"

/* SDL3 playback shared by the desktop and browser frontends. */
void zvb_audio_init(zvb_sound_t* sound);
void zvb_audio_deinit(zvb_sound_t* sound);
void zvb_audio_set_gain(float gain);
