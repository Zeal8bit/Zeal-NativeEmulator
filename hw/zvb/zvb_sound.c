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

static inline bool voice_in_left(zvb_sound_t* sound, int i)
{
    return (sound->left_voices & BIT(i)) != 0;
}

static inline bool voice_in_right(zvb_sound_t* sound, int i)
{
    return (sound->right_voices & BIT(i)) != 0;
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
    sound->left_volume = 0.f;
    sound->right_volume = 0.f;
    sound->sample_table.baud_count = 0;
    atomic_init(&sound->sample_table.output, 0);
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
        sound->voices[i] = (zvb_voice_t) { 0 };
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
    atomic_store(&sound->sample_table.output, 0);
    output_lock(sound);
    sound->output_head = sound->output_tail = sound->output_count = 0;
    output_unlock(sound);

    /* Internal registers */
    sound->left_volume = 0.f;
    sound->right_volume = 0.f;
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

/**
 * @brief Generate a 16-bit unsigned sample for the current voice
 */
static int16_t generate_wave(zvb_voice_t* voice) {
    const int steps = (voice->freq_high << 8) | voice->freq_low;
    if (steps == 0) {
        return 0;
    }

    uint_fast16_t sample = 0;
    /* The duty value represents the upper 3 bits of the 16-bit value */
    const uint_fast16_t threshold = voice->duty << 13;

    switch (voice->wave) {
        case WAVE_SQUARE:
            sample = (voice->phase < threshold) ? SAMPLE_MAX : 0;
            break;
        case WAVE_TRIANGLE:
            sample = (voice->phase > SAMPLE_MAX / 2) ?
                        SAMPLE_MAX - voice->phase : voice->phase;
            sample *= 2;
            break;
        case WAVE_SAWTOOTH:
            sample = voice->phase;
            break;
        case WAVE_NOISE:
            sample = rand() % SAMPLE_MAX;
            break;
    }

    if (!voice->hold) {
        voice->phase += steps;
    }
    if (voice->phase > SAMPLE_MAX) {
        voice->phase = steps;
    }

    return (sample * voice->volume) - 0x8000;
}

/**
 * @brief Generate the next sample for the sample-table voice
 */
void zvb_sound_clock(zvb_sound_t* sound)
{
    zvb_sample_table_t* tbl = &sound->sample_table;
    const uint8_t state = tbl->state;
    const uint8_t ram_output = tbl->ram_output;
    tbl->ram_output = tbl->fifo[tbl->fifo_tail];
    const bool sample_clock = sound->sample_clock_counter == 1132;
    /* The FPGA mixer samples the old wavetable state one clock later. */
    if (sound->sample_clock_counter == 1133 && sound->enabled) {
        output_lock(sound);
        if (sound->output_count < SAMPLE_OUTPUT_SIZE) {
            sound->output_samples[sound->output_head] = atomic_load(&tbl->output) - 0x8000;
            sound->output_head = (sound->output_head + 1) % SAMPLE_OUTPUT_SIZE;
            sound->output_count++;
        }
        output_unlock(sound);
    }
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
    int16_t *buffer = (int16_t*) rbuf;

    for (unsigned int i = 0; i < frames * 2; i += SOUND_CHANNELS) {
        int sample_left = 0;
        int sample_right = 0;

        for (int ch = 0; ch < VOICE_COUNT; ch++) {
            int16_t sample = generate_wave(&g_sound->voices[ch]);
            if (voice_in_left(g_sound, ch)) sample_left += sample;
            if (voice_in_right(g_sound, ch)) sample_right += sample;
        }

        output_lock(g_sound);
        int sample = atomic_load(&g_sound->sample_table.output) - 0x8000;
        if (g_sound->output_count) {
            sample = g_sound->output_samples[g_sound->output_tail];
            g_sound->output_tail = (g_sound->output_tail + 1) % SAMPLE_OUTPUT_SIZE;
            g_sound->output_count--;
        }
        output_unlock(g_sound);
        if (voice_in_left(g_sound, 7)) sample_left += sample;
        if (voice_in_right(g_sound, 7)) sample_right += sample;

        /* Apply master volume */
        /* No matter how many samples are enabled, divide by VOICE_COUNT and make it signed */
        sample_left = (sample_left / VOICE_COUNT) * g_sound->left_volume;
        sample_right = (sample_right / VOICE_COUNT) * g_sound->right_volume;

        buffer[i]   = (int16_t) sample_left;
        buffer[i+1] = (int16_t) sample_right;
    }
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
                }
            }

            if (sample_table_enabled(sound)) {
                tbl->divider = value & 7;
            }
            break;

        case REG_WAVEFORM:
            for (int i = 0; i < VOICE_COUNT; i++) {
                if (voice_enabled(sound, i)) {
                    sound->voices[i].wave = value & 0x3;
                    sound->voices[i].duty = value >> REG_WAVEFORM_DUTY_SH;
                    sound->voices[i].noise = (sound->voices[i].wave == WAVE_NOISE);
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
                    sound->voices[i].volume = volume_steps_to_float(value, 2);
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
            sound->master_volume = value;
            if (value & 0x80) {
                sound->right_volume = 0.f;
            } else {
                /* We have two bits for volume */
                sound->right_volume = volume_steps_to_float(value >> 2, 2);
            }
            if (value & 0x40) {
                sound->left_volume = 0.f;
            } else {
                sound->left_volume = volume_steps_to_float(value, 2);
            }
            break;

        case REG_MST_ENA:
            sound->enabled_voices = value;
            break;

        default:
            break;
    }
}
