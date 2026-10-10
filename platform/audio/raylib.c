/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file raylib.c
 * @brief Raylib implementation of the audio seam, used by the WebAssembly build.
 */
#include "raylib.h"
#include <stddef.h>
#include "platform/audio.h"

static AudioStream stream;
static audio_callback_t producer;
static bool open;
static float current_volume = 1.0f;

/* Raylib's callback has no opaque parameter, so the seam's producer is forwarded. */
static void trampoline(void *buffer, unsigned int frames)
{
    if (producer) {
        producer(buffer, frames);
    }
}

bool audio_open(int sample_rate, int channels, audio_callback_t callback)
{
    if (callback == NULL) {
        return false;
    }
    producer = callback;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        producer = NULL;
        return false;
    }
    stream = LoadAudioStream((unsigned int)sample_rate, 16, (unsigned int)channels);
    SetAudioStreamCallback(stream, trampoline);
    PlayAudioStream(stream);
    open = true;
    audio_set_volume(current_volume);
    return true;
}

void audio_close(void)
{
    if (open) {
        StopAudioStream(stream);
        UnloadAudioStream(stream);
        open = false;
    }
    producer = NULL;
    CloseAudioDevice();
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
    SetMasterVolume(volume);
}

float audio_volume(void) { return current_volume; }
