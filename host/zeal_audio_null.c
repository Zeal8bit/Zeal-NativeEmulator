/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_audio_null.c
 * @brief Silent audio seam, for builds with no sound device.
 *
 * Selecting this backend lets the emulator build and run without any audio library;
 * it simply never produces sound.
 */
#include <stddef.h>
#include "host/zeal_audio.h"

static float current_volume = 1.0f;

bool zeal_audio_open(int sample_rate, int channels, zeal_audio_callback_t callback)
{
    (void)sample_rate;
    (void)channels;
    (void)callback;
    return false;
}

void zeal_audio_close(void) {}

bool zeal_audio_ready(void) { return false; }

void zeal_audio_set_volume(float volume)
{
    if (volume < 0.0f) {
        volume = 0.0f;
    }
    if (volume > 1.0f) {
        volume = 1.0f;
    }
    current_volume = volume;
}

float zeal_audio_volume(void) { return current_volume; }
