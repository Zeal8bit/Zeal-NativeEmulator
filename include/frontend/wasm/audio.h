/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "raylib.h"
#include "hw/zvb/zvb_sound.h"

typedef struct {
    AudioStream stream;
    bool enabled;
    /* Host playback queue, separate from the emulated hardware FIFO. */
    atomic_flag output_lock;
    zvb_pcm_frame_t output_samples[SAMPLE_OUTPUT_SIZE];
    uint32_t output_head;
    uint32_t output_tail;
    uint32_t output_count;
    /* Host-only resampling state; never feeds back into FPGA synthesis. */
    zvb_pcm_frame_t playback_last;
    bool playback_started;
    double playback_position;
    double playback_rate;
    /* Host AC coupling, after resampling; protected by output_lock. */
    double playback_dc_input[SOUND_CHANNELS];
    double playback_dc_output[SOUND_CHANNELS];
    bool playback_dc_ready;
} wasm_audio_t;

void wasm_audio_init(zvb_sound_t* sound, bool enabled);
void wasm_audio_deinit(zvb_sound_t* sound);
