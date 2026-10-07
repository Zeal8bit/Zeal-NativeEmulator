/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "hw/zvb/zvb_sound.h"

#define BIT(i)  (1 << (i))
#ifndef MAX
#define MAX(a,b)    ((a) > (b) ? (a) : (b))
#endif

static inline bool sample_table_enabled(zvb_sound_t* sound)
{
    return (sound->enabled_voices & 0x80) != 0;
}

static inline bool voice_enabled(zvb_sound_t* sound, int i)
{
    return (sound->enabled_voices & BIT(i)) != 0;
}

static inline bool voice_held(zvb_sound_t* sound, int i)
{
    return (sound->hold_voices & BIT(i)) != 0;
}

static void audio_callback(void *buffer, unsigned int frames);

static zvb_sound_t* g_sound;

static void output_lock(zvb_sound_t* sound)
{
    while (atomic_flag_test_and_set_explicit(&sound->output_lock, memory_order_acquire)) {}
}

static void output_unlock(zvb_sound_t* sound)
{
    atomic_flag_clear_explicit(&sound->output_lock, memory_order_release);
}

void zvb_sound_init(zvb_sound_t* sound, bool enabled)
{
    assert(sound);
    memset(sound, 0, sizeof(*sound));
    sound->enabled = enabled;
    sound->sample_table.baud_count = 0;
    atomic_init(&sound->sample_table.output, 0);
    for (int i = 0; i < VOICE_COUNT; i++) atomic_init(&sound->voices[i].output, 0);
    sound->lfsr = 0x8898;
    sound->master_volume = 0xc0;
    atomic_flag_clear(&sound->output_lock);

    if (!enabled) {
        return;
    }

    InitAudioDevice();

    /* Dirty hack but the callback doesn't take a context/opaque parameter... */
    g_sound = sound;

    SetMasterVolume(1.0f);
    sound->stream = LoadAudioStream(SAMPLE_RATE, 16, SOUND_CHANNELS);
    SetAudioStreamCallback(sound->stream, audio_callback);
    PlayAudioStream(sound->stream);
}

void zvb_sound_reset(zvb_sound_t* sound)
{
    /* Master registers */
    sound->hold_voices    = 0;
    sound->enabled_voices = 0;
    sound->left_voices    = 0;
    sound->right_voices   = 0;
    /* Both channels disabled */
    sound->master_volume  = 0xc0;

    /* Voices registers */
    for (int i = 0; i < VOICE_COUNT; i++) {
        zvb_voice_t* voice = &sound->voices[i];
        voice->wave = voice->duty = voice->voice_volume = 0;
        voice->frequency = voice->phase = 0;
        voice->hold = voice->need_reload = voice->decrementing = false;
        /* Frequency/waveform write latches are retained on RTL reset. */
        atomic_store_explicit(&voice->output, 0, memory_order_relaxed);
    }
    /* WaveTable.v resets equal pointers with empty=false (full). */
    sound->sample_table.fifo_empty = false;
    sound->sample_table.baud_count = 0;
    sound->sample_table.is_signed  = false;
    sound->sample_table.fifo_head  = 0;
    sound->sample_table.fifo_tail  = 0;
    sound->sample_table.divider = 0;
    sound->sample_table.config  = 0;
    sound->sample_table.is_u8   = false;
    sound->sample_table.hold    = false;
    sound->sample_table.int_pending = false;
    sound->sample_table.state = 0;
    sound->sample_clock_counter = 0;
    sound->lfsr = 0x8898;
    atomic_store(&sound->sample_table.output, 0);
    output_lock(sound);
    sound->output_head = sound->output_tail = sound->output_count = 0;
    sound->output_last = (zvb_pcm_frame_t){0};
    sound->mix_state = 0;
    output_unlock(sound);

}

void zvb_sound_deinit(zvb_sound_t* sound)
{
    if (sound == NULL || !sound->enabled) {
        return;
    }

    StopAudioStream(sound->stream);
    UnloadAudioStream(sound->stream);
    CloseAudioDevice();
    sound->enabled = false;
    g_sound = NULL;
}

/* Integer scaling matches the RTL's separate truncating shifts. */
static uint16_t scale_volume(uint16_t value, unsigned volume)
{
    const unsigned factor = (volume & 3) + 1;
    return ((factor & 4) ? value : 0) +
           ((factor & 2) ? value >> 1 : 0) +
           ((factor & 1) ? value >> 2 : 0);
}

static void voice_clock(zvb_voice_t* voice, bool sample_clock, uint16_t lfsr)
{
    const uint16_t output = (voice->voice_volume & 0x80) ? 0 :
                            scale_volume(voice->max_state, voice->voice_volume);
    if (output != atomic_load_explicit(&voice->output, memory_order_relaxed))
        atomic_store_explicit(&voice->output, output, memory_order_relaxed);
    /* SoundVoice.v has two output pipeline registers, both updated each clock. */
    voice->max_state = voice->wave == WAVE_SQUARE ?
                       (voice->phase >= ((unsigned)voice->duty << 13) ? 65535 : 0) :
                       (voice->phase & 0x10000 ? 65535 : voice->phase);
    if (!sample_clock || voice->hold) return;
    const unsigned previous_phase = voice->phase;
    const unsigned next = (previous_phase + voice->frequency) & 0x1ffff;
    const unsigned triangle = (previous_phase +
        (voice->decrementing ? -(int)voice->frequency * 2 : voice->frequency * 2)) & 0x1ffff;
    switch (voice->wave) {
        case WAVE_SQUARE: case WAVE_SAWTOOTH:
            voice->phase = next & 0x10000 ? voice->frequency : next;
            break;
        case WAVE_TRIANGLE:
            if ((!voice->decrementing && (previous_phase & 0x10000)) ||
                (voice->decrementing && !triangle))
                voice->decrementing = !voice->decrementing;
            voice->phase = triangle;
            break;
        case WAVE_NOISE: voice->phase = lfsr; break;
    }
    if (voice->need_reload && ((voice->voice_volume & 0x80) ||
        voice->wave == WAVE_NOISE || !previous_phase ||
        ((next & 0x10000) && (voice->wave == WAVE_SQUARE || voice->wave == WAVE_SAWTOOTH)) ||
        (!triangle && voice->wave == WAVE_TRIANGLE))) {
        voice->frequency = ((uint16_t)voice->freq_high << 8) | voice->freq_low;
        voice->wave = voice->wave_latch;
        voice->duty = voice->duty_latch;
        voice->phase = 0;
        voice->decrementing = voice->need_reload = false;
    }
}

static uint16_t mix_mean(const zvb_sound_t* sound, unsigned routes)
{
    unsigned sum = routes & 0x80 ? atomic_load_explicit(&sound->sample_table.output, memory_order_relaxed) : 0;
    for (int i = 0; i < VOICE_COUNT; i++)
        if (routes & (1u << i)) sum += atomic_load_explicit(&sound->voices[i].output, memory_order_relaxed);
    return sum & (1u << 18) ? 65535 : sum >> 2;
}

static void mixer_clock(zvb_sound_t* sound)
{
    /* ZealSound.v captures sums one clock after samples_ready, and applies
     * master volume on the following clock. Read old voice pipeline outputs. */
    if (sound->mix_state == 1) {
        sound->mean_left = mix_mean(sound, sound->left_voices);
        sound->mean_right = mix_mean(sound, sound->right_voices);
        sound->mix_state = 2;
    } else if (sound->mix_state == 2) {
        const unsigned volume = sound->master_volume;
        const uint16_t left = volume & 0x40 ? 0 : scale_volume(sound->mean_left, volume);
        const uint16_t right = volume & 0x80 ? 0 : scale_volume(sound->mean_right, volume >> 2);
        const zvb_pcm_frame_t frame = { .left = (int)left - 32768, .right = (int)right - 32768 };
        output_lock(sound);
        sound->output_last = frame;
        if (sound->enabled && sound->output_count < SAMPLE_OUTPUT_SIZE) {
            sound->output_samples[sound->output_head] = frame;
            sound->output_head = (sound->output_head + 1) % SAMPLE_OUTPUT_SIZE;
            sound->output_count++;
        }
        output_unlock(sound);
        sound->mix_state = 0;
    }
    if (sound->sample_clock_counter == 1133) sound->mix_state = 1;
}

/**
 * @brief Generate the next sample for the sample-table voice
 */
void zvb_sound_clock(zvb_sound_t* sound)
{
    mixer_clock(sound);
    zvb_sample_table_t* tbl = &sound->sample_table;
    const uint8_t state = tbl->state;
    const uint8_t ram_output = tbl->ram_output;
    tbl->ram_output = tbl->fifo[tbl->fifo_tail];
    const bool sample_clock = sound->sample_clock_counter == 1132;
    for (int i = 0; i < VOICE_COUNT; i++) voice_clock(&sound->voices[i], sample_clock, sound->lfsr);
    const uint16_t feedback = (sound->lfsr ^ (sound->lfsr >> 2) ^
                               (sound->lfsr >> 3) ^ (sound->lfsr >> 5)) & 1;
    sound->lfsr = (sound->lfsr >> 1) | (feedback << 15);
    sound->sample_clock_counter = (sound->sample_clock_counter + 1) % 1134;
    if (sample_clock && !tbl->hold) {
        if (tbl->baud_count >= tbl->divider) {
            tbl->baud_count = 0;
            if (tbl->fifo_empty) {
                atomic_store(&tbl->output, 0);
            } else {
                tbl->state = 1;
                tbl->fifo_tail = (tbl->fifo_tail + 1) % SAMPLE_FIFO_SIZE;
            }
        } else {
            tbl->baud_count++;
        }
    }
    switch (state) {
        case 1:
            tbl->sample_output = (tbl->sample_output & 0xff00) | ram_output;
            tbl->state = 2;
            break;
        case 2:
            tbl->sample_output = (tbl->sample_output & 255) |
                ((uint16_t)(tbl->is_u8 ? tbl->sample_output & 255 : ram_output) << 8);
            if (!tbl->is_u8) tbl->fifo_tail = (tbl->fifo_tail + 1) % SAMPLE_FIFO_SIZE;
            tbl->state = 3;
            break;
        case 3: {
            tbl->fifo_empty = tbl->fifo_tail == tbl->fifo_head && !(tbl->config & 2);
            if (tbl->fifo_empty) tbl->int_pending = true;
            const uint16_t sample = tbl->sample_output +
                (tbl->is_signed && !tbl->is_u8 ? 0x8000 : 0);
            atomic_store(&tbl->output, sample);
            tbl->state = 0;
            break;
        }
        default: break;
    }
}


static void audio_callback(void* rbuf, unsigned int frames)
{
    int16_t* buffer = rbuf;
    /* The host only consumes complete frames; all registers and synthesis
     * belong to the emulation thread. */
    output_lock(g_sound);
    for (unsigned int i = 0; i < frames; i++) {
        zvb_pcm_frame_t sample = g_sound->output_last;
        if (g_sound->output_count) {
            sample = g_sound->output_samples[g_sound->output_tail];
            g_sound->output_tail = (g_sound->output_tail + 1) % SAMPLE_OUTPUT_SIZE;
            g_sound->output_count--;
        }
        buffer[i * SOUND_CHANNELS] = sample.left;
        buffer[i * SOUND_CHANNELS + 1] = sample.right;
    }
    output_unlock(g_sound);
}


uint8_t zvb_sound_read(zvb_sound_t* sound, uint32_t port) {
    if (!sound) {
        return 0;
    }
    zvb_sample_table_t* tbl = &sound->sample_table;

    switch (port) {
        case 1:
            if (sample_table_enabled(sound)) {
                return tbl->divider;
            }
            break;
        case 2:
            if (sample_table_enabled(sound)) {
                const uint8_t status =
                    (tbl->fifo_empty << 7) |
                    ((tbl->fifo_head == tbl->fifo_tail && !tbl->fifo_empty) << 6) |
                    (tbl->config & 0xf);
                return status;
            }
            break;
        case 3:
            if (sample_table_enabled(sound)) return tbl->int_pending;
            break;
        case REG_MST_LEFT:  return sound->left_voices;
        case REG_MST_RIGHT: return sound->right_voices;
        case REG_MST_HOLD:  return sound->hold_voices;
        case REG_MST_VOL:   return sound->master_volume;
        case REG_MST_ENA:   return sound->enabled_voices;
        default:            return 0;
    }

    return 0;
}


void zvb_sound_write(zvb_sound_t* sound, uint32_t port, uint8_t value) {
    if (!sound) {
        return;
    }
    zvb_sample_table_t* tbl = &sound->sample_table;

    switch (port) {
        case REG_FREQ_LOW:
            for (int i = 0; i < VOICE_COUNT; i++) {
                if (voice_enabled(sound, i)) {
                    sound->voices[i].freq_low = value;
                }
            }
            if (sample_table_enabled(sound)) {
                /* Register 0 corresponds to the FIFO */
                tbl->fifo[tbl->fifo_head] = value;
                tbl->fifo_head = (tbl->fifo_head + 1) % SAMPLE_FIFO_SIZE;
                tbl->fifo_empty = false;
            }
            break;

        case REG_FREQ_HIGH:
            for (int i = 0; i < VOICE_COUNT; i++) {
                if (voice_enabled(sound, i)) {
                    sound->voices[i].freq_high = value;
                    sound->voices[i].need_reload = true;
                }
            }

            if (sample_table_enabled(sound)) {
                tbl->divider = value & 7;
            }
            break;

        case REG_WAVEFORM:
            for (int i = 0; i < VOICE_COUNT; i++) {
                if (voice_enabled(sound, i)) {
                    sound->voices[i].wave_latch = value & 0x3;
                    sound->voices[i].duty_latch = value >> REG_WAVEFORM_DUTY_SH;
                    sound->voices[i].need_reload = true;
                }
            }
            /* Special case for the sample table voice,
             * Register 2 corresponds to the configuration */
            if (sample_table_enabled(sound)) {
                tbl->config    = value & 0xf;
                tbl->is_u8     = (value & 1) != 0;
                tbl->is_signed = (value & 4) != 0;
            }
            break;

        case REG_VOICE_VOL:
            if (sample_table_enabled(sound) && (value & 1)) tbl->int_pending = false;
            for (int i = 0; i < VOICE_COUNT; i++) {
                if (voice_enabled(sound, i)) {
                    sound->voices[i].voice_volume = value;
                }
            }
            break;

        case REG_MST_LEFT:
            sound->left_voices = value;
            break;

        case REG_MST_RIGHT:
            sound->right_voices = value;
            break;

        case REG_MST_HOLD:
            sound->hold_voices = value;
            for (int i = 0; i < VOICE_COUNT; i++) {
                sound->voices[i].hold = voice_held(sound, i);
            }
            /* If the wavetable is on hold, it should stop outputting sound */
            sound->sample_table.hold = voice_held(sound, 7);
            break;

        case REG_MST_VOL:
            sound->master_volume = value & 0xcf;
            break;

        case REG_MST_ENA:
            sound->enabled_voices = value;
            break;

        default:
            break;
    }
}
