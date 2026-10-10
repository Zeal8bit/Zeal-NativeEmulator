/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file sdl.c
 * @brief SDL2 implementation of the audio seam, used by desktop builds.
 *
 * SDL owns the device and calls back for samples; volume is applied here because the
 * seam hands the producer a plain buffer to fill.
 */
#include <SDL.h>
#include <stddef.h>
#include "platform/audio.h"

static SDL_AudioDeviceID device;
static audio_callback_t producer;
static int channel_count = 2;
static bool open;
static float current_volume = 1.0f;

static void apply_volume(Uint8 *stream, int bytes)
{
    if (current_volume >= 1.0f || bytes <= 0) {
        return;
    }
    int16_t *samples = (int16_t *)stream;
    const int count = bytes / (int)sizeof(int16_t);
    for (int i = 0; i < count; i++) {
        samples[i] = (int16_t)(samples[i] * current_volume);
    }
}

static void sdl_audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    if (producer == NULL) {
        SDL_memset(stream, 0, (size_t)len);
        return;
    }
    const int bytes_per_frame = (int)sizeof(int16_t) * channel_count;
    producer(stream, (unsigned int)(len / bytes_per_frame));
    apply_volume(stream, len);
}

bool audio_open(int sample_rate, int channels, audio_callback_t callback)
{
    if (callback == NULL || channels <= 0) {
        return false;
    }
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        return false;
    }
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    SDL_zero(want);
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)channels;
    want.samples = 1024;
    want.callback = sdl_audio_callback;
    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (device == 0) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    channel_count = channels;
    producer = callback;
    open = true;
    SDL_PauseAudioDevice(device, 0);
    return true;
}

void audio_close(void)
{
    if (open) {
        SDL_CloseAudioDevice(device);
        open = false;
    }
    producer = NULL;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool audio_ready(void) { return open; }

void audio_set_volume(float volume)
{
    if (volume < 0.0f) {
        volume = 0.0f;
    }
    if (volume > 1.0f) {
        volume = 1.0f;
    }
    current_volume = volume;
}

float audio_volume(void) { return current_volume; }
