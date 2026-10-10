/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file zeal_audio.h
 * @brief Audio output seam: what the emulator needs from the machine's sound device.
 *
 * This is a compile-time seam like include/host/zeal_host.h. Exactly one backend is
 * linked into a binary, so the emulator never includes a sound library:
 *
 *   - host/zeal_audio_raylib.c -> Raylib's audio (WebAssembly)
 *   - host/zeal_audio_null.c   -> silent, for builds with no audio device
 *
 * A different backend (SDL, PulseAudio, ALSA, ...) only has to implement the four
 * functions below; nothing else in the emulator changes.
 */
#ifndef ZEAL_AUDIO_H
#define ZEAL_AUDIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Called from the backend's audio thread whenever it needs more samples.
 *
 * The callback must fill @p frames frames of interleaved 16-bit samples into
 * @p buffer and must not block. @c buffer is laid out as the channel count that was
 * passed to zeal_audio_open().
 */
typedef void (*zeal_audio_callback_t)(void *buffer, unsigned int frames);

/**
 * @brief Open the sound device and start pulling from @p callback.
 *
 * @param sample_rate Samples per second.
 * @param channels    Interleaved channel count (1 mono, 2 stereo).
 * @param callback    Sample producer, called on the backend's audio thread.
 *
 * @return true on success. On failure the emulator keeps running silently, so a
 *         missing sound device is never fatal.
 */
bool zeal_audio_open(int sample_rate, int channels, zeal_audio_callback_t callback);

/**
 * @brief Stop the device and release it. Safe to call when it was never opened.
 */
void zeal_audio_close(void);

/**
 * @brief True while the device is open and pulling samples.
 */
bool zeal_audio_ready(void);

/**
 * @brief Set the output volume, clamped to 0.0 (silent) through 1.0 (full scale).
 */
void zeal_audio_set_volume(float volume);

/**
 * @brief Current volume in the range 0.0 to 1.0.
 */
float zeal_audio_volume(void);

#ifdef __cplusplus
}
#endif

#endif /* ZEAL_AUDIO_H */
