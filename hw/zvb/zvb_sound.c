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

#define SOUND_NOISE_SEED                 (0x8898)
#define SOUND_VOICE_MUTE                 (0x80)
#define SOUND_PHASE_MASK                 (0x1ffff)
#define SOUND_PHASE_OVERFLOW             (0x10000)
#define SOUND_CHANNEL_MUTE_LEFT          (0x40)
#define SOUND_CHANNEL_MUTE_RIGHT         (0x80)
#define SOUND_CHANNEL_MUTE_BOTH          (0xc0)
#define SOUND_SAMPLE_ROUTE               (0x80)
#define SOUND_SAMPLE_READY_CLOCK         (1132)
#define SOUND_MIX_CAPTURE_CLOCK          (1133)
#define SOUND_SAMPLE_PERIOD_CLOCKS       (1134)
#define SOUND_FIFO_IRQ_ENABLE            (8)
#define SOUND_FIFO_PIPELINE_END          (4)
#define SOUND_PCM_ZERO                   (32768)
#define SOUND_PCM_MAX                    (65535)
#define SOUND_NOISE_BITS                 (16)
#define SOUND_NOISE_JUMP_BITS            (11)
#define SOUND_NOISE_NIBBLE_BITS          (4)
#define SOUND_NOISE_NIBBLE_COUNT         (SOUND_NOISE_BITS / SOUND_NOISE_NIBBLE_BITS)
#define SOUND_NOISE_NIBBLE_VALUES        (1u << SOUND_NOISE_NIBBLE_BITS)
#define SOUND_NOISE_NIBBLE_MASK          (SOUND_NOISE_NIBBLE_VALUES - 1u)
#define SOUND_VOICE_PIPELINE_CLOCKS      (2)

_Static_assert(SOUND_SAMPLE_READY_CLOCK < (1u << SOUND_NOISE_JUMP_BITS),
               "Noise jumps must cover the interval between samples");

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

static void audio_callback(void *buffer, uint32_t frames);

static zvb_sound_t* g_sound;

/* Nibble lookups preserve exact linear noise transforms between samples. */
static uint16_t noise_nibbles[SOUND_NOISE_JUMP_BITS][SOUND_NOISE_NIBBLE_COUNT][SOUND_NOISE_NIBBLE_VALUES];

static uint16_t noise_clock(uint16_t state)
{
    const uint16_t feedback = (state ^ (state >> 2) ^
                               (state >> 3) ^ (state >> 5)) & 1;
    return (state >> 1) | (feedback << (SOUND_NOISE_BITS - 1));
}

static uint16_t noise_transform(uint16_t state, const uint16_t* transform)
{
    uint16_t result = 0;
    for (unsigned bit = 0; state != 0; bit++) {
        if (state & 1) {
            result ^= transform[bit];
        }
        state >>= 1;
    }
    return result;
}

static void noise_jump_init(void)
{
    uint16_t noise_jump[SOUND_NOISE_JUMP_BITS][SOUND_NOISE_BITS];
    for (unsigned bit = 0; bit < SOUND_NOISE_BITS; bit++) {
        noise_jump[0][bit] = noise_clock(1u << bit);
    }
    for (unsigned power = 1; power < SOUND_NOISE_JUMP_BITS; power++) {
        for (unsigned bit = 0; bit < SOUND_NOISE_BITS; bit++) {
            noise_jump[power][bit] = noise_transform(noise_jump[power - 1][bit],
                                                   noise_jump[power - 1]);
        }
    }
    for (uint32_t power = 0; power < SOUND_NOISE_JUMP_BITS; power++) {
        for (uint32_t chunk = 0; chunk < SOUND_NOISE_NIBBLE_COUNT; chunk++) {
            for (uint32_t value = 0; value < SOUND_NOISE_NIBBLE_VALUES; value++) {
                noise_nibbles[power][chunk][value] =
                    noise_transform(value << (chunk * SOUND_NOISE_NIBBLE_BITS), noise_jump[power]);
            }
        }
    }
}

static uint16_t noise_advance(uint16_t state, uint32_t clocks)
{
    for (unsigned power = 0; clocks != 0; power++) {
        if (clocks & 1) {
            state = noise_nibbles[power][0][state & SOUND_NOISE_NIBBLE_MASK] ^
                    noise_nibbles[power][1][(state >> SOUND_NOISE_NIBBLE_BITS) & SOUND_NOISE_NIBBLE_MASK] ^
                    noise_nibbles[power][2][(state >> (2 * SOUND_NOISE_NIBBLE_BITS)) & SOUND_NOISE_NIBBLE_MASK] ^
                    noise_nibbles[power][3][state >> (3 * SOUND_NOISE_NIBBLE_BITS)];
        }
        clocks >>= 1;
    }
    return state;
}


static void output_lock(zvb_sound_t* sound)
{
    while (atomic_flag_test_and_set_explicit(&sound->output_lock, memory_order_acquire)) {
    }
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
    for (int i = 0; i < VOICE_COUNT; i++) {
        atomic_init(&sound->voices[i].output, 0);
    }
    sound->lfsr = SOUND_NOISE_SEED;
    sound->settle_clocks = SOUND_VOICE_PIPELINE_CLOCKS;
    noise_jump_init();
    sound->master_volume = SOUND_CHANNEL_MUTE_BOTH;
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
        voice->voice_volume = 0;
        voice->duty = 0;
        voice->wave = 0;
        voice->phase = 0;
        voice->frequency = 0;
        voice->decrementing = false;
        voice->need_reload = false;
        voice->hold = false;
        /* Reset retains the frequency/waveform write latches. */
        atomic_store_explicit(&voice->output, 0, memory_order_relaxed);
    }
    /* Reset starts with equal FIFO pointers and a full FIFO. */
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
    sound->lfsr = SOUND_NOISE_SEED;
    sound->settle_clocks = SOUND_VOICE_PIPELINE_CLOCKS;
    atomic_store(&sound->sample_table.output, 0);
    output_lock(sound);
    sound->output_count = 0;
    sound->output_tail = 0;
    sound->output_head = 0;
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

/* Apply each truncating shift before summing the scaled values. */
static uint16_t scale_volume(uint16_t value, uint32_t volume)
{
    const uint32_t factor = (volume & 3) + 1;
    return ((factor & 4) ? value : 0) +
           ((factor & 2) ? value >> 1 : 0) +
           ((factor & 1) ? value >> 2 : 0);
}

static void voice_clock(zvb_voice_t* voice, bool sample_clock, uint16_t lfsr)
{
    const uint16_t output = (voice->voice_volume & SOUND_VOICE_MUTE) ? 0 :
                            scale_volume(voice->max_state, voice->voice_volume);
    if (output != atomic_load_explicit(&voice->output, memory_order_relaxed)) {
        atomic_store_explicit(&voice->output, output, memory_order_relaxed);
    }
    /* Update both voice output pipeline stages each clock. */
    voice->max_state = voice->wave == WAVE_SQUARE ?
                       (voice->phase >= ((uint32_t)voice->duty << 13) ? SOUND_PCM_MAX : 0) :
                       (voice->phase & SOUND_PHASE_OVERFLOW ? SOUND_PCM_MAX : voice->phase);
    if (!sample_clock || voice->hold) {
        return;
    }
    const uint32_t previous_phase = voice->phase;
    const uint32_t next = (previous_phase + voice->frequency) & SOUND_PHASE_MASK;
    const uint32_t triangle = (previous_phase +
        (voice->decrementing ? -(int)voice->frequency * 2 : voice->frequency * 2)) & SOUND_PHASE_MASK;
    switch (voice->wave) {
        case WAVE_SQUARE:
        case WAVE_SAWTOOTH:
            voice->phase = next & SOUND_PHASE_OVERFLOW ? voice->frequency : next;
            break;
        case WAVE_TRIANGLE:
            if ((!voice->decrementing && (previous_phase & SOUND_PHASE_OVERFLOW)) ||
                (voice->decrementing && !triangle)) {
                voice->decrementing = !voice->decrementing;
            }
            voice->phase = triangle;
            break;
        case WAVE_NOISE:
            voice->phase = lfsr;
            break;
    }
    if (voice->need_reload && ((voice->voice_volume & SOUND_VOICE_MUTE) ||
        voice->wave == WAVE_NOISE || !previous_phase ||
        ((next & SOUND_PHASE_OVERFLOW) && (voice->wave == WAVE_SQUARE || voice->wave == WAVE_SAWTOOTH)) ||
        (!triangle && voice->wave == WAVE_TRIANGLE))) {
        voice->frequency = ((uint16_t)voice->freq_high << 8) | voice->freq_low;
        voice->wave = voice->wave_latch;
        voice->duty = voice->duty_latch;
        voice->phase = 0;
        voice->need_reload = false;
        voice->decrementing = false;
    }
}

static uint16_t mix_mean(const zvb_sound_t* sound, uint32_t routes)
{
    uint32_t sum = routes & SOUND_SAMPLE_ROUTE ? atomic_load_explicit(&sound->sample_table.output, memory_order_relaxed) : 0;
    for (int i = 0; i < VOICE_COUNT; i++) {
        if (routes & (1u << i)) {
            sum += atomic_load_explicit(&sound->voices[i].output, memory_order_relaxed);
        }
    }
    return sum & (1u << 18) ? SOUND_PCM_MAX : sum >> 2;
}

static void mixer_clock(zvb_sound_t* sound)
{
    /* Capture sums one clock after samples_ready, and apply
     * master volume on the following clock. Read old voice pipeline outputs. */
    if (sound->mix_state == 1) {
        sound->mean_left = mix_mean(sound, sound->left_voices);
        sound->mean_right = mix_mean(sound, sound->right_voices);
        sound->mix_state = 2;
    } else if (sound->mix_state == 2) {
        const uint32_t volume = sound->master_volume;
        const uint16_t left = volume & SOUND_CHANNEL_MUTE_LEFT ? 0 : scale_volume(sound->mean_left, volume);
        const uint16_t right = volume & SOUND_CHANNEL_MUTE_RIGHT ? 0 : scale_volume(sound->mean_right, volume >> 2);
        const zvb_pcm_frame_t frame = { .left = (int)left - SOUND_PCM_ZERO, .right = (int)right - SOUND_PCM_ZERO };
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
    if (sound->sample_clock_counter == SOUND_MIX_CAPTURE_CLOCK) {
        sound->mix_state = 1;
    }
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
    const bool sample_clock = sound->sample_clock_counter == SOUND_SAMPLE_READY_CLOCK;
    for (int i = 0; i < VOICE_COUNT; i++) {
        voice_clock(&sound->voices[i], sample_clock, sound->lfsr);
    }
    if (sample_clock) {
        sound->settle_clocks = SOUND_VOICE_PIPELINE_CLOCKS;
    } else if (sound->settle_clocks != 0) {
        sound->settle_clocks--;
    }
    sound->lfsr = noise_clock(sound->lfsr);
    sound->sample_clock_counter = (sound->sample_clock_counter + 1) % SOUND_SAMPLE_PERIOD_CLOCKS;
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
            if (!tbl->is_u8) {
                tbl->fifo_tail = (tbl->fifo_tail + 1) % SAMPLE_FIFO_SIZE;
            }
            tbl->state = 3;
            break;
        case 3: {
            tbl->fifo_empty = tbl->fifo_tail == tbl->fifo_head && !(tbl->config & 2);
            if (tbl->fifo_empty) {
                tbl->int_pending = true;
            }
            const uint16_t sample = tbl->sample_output +
                (tbl->is_signed && !tbl->is_u8 ? 0x8000 : 0);
            atomic_store(&tbl->output, sample);
            tbl->state = 0;
            break;
        }
        default:
            break;
    }
}


/* Settle changed outputs, then skip clocks with no sample or pipeline work.
 * Noise advances through the same states without visiting each master clock. */
void zvb_sound_advance(zvb_sound_t* sound, uint32_t clocks)
{
    while (clocks != 0) {
        if (sound->settle_clocks != 0 || sound->sample_table.state != 0 ||
            sound->mix_state != 0 || sound->sample_clock_counter >= SOUND_SAMPLE_READY_CLOCK) {
            zvb_sound_clock(sound);
            clocks--;
            continue;
        }
        uint32_t skip = SOUND_SAMPLE_READY_CLOCK - sound->sample_clock_counter;
        if (skip > clocks) {
            skip = clocks;
        }
        sound->lfsr = noise_advance(sound->lfsr, skip);
        sound->sample_clock_counter += skip;
        clocks -= skip;
    }
}


uint32_t zvb_sound_clocks_until_interrupt(const zvb_sound_t* sound)
{
    const zvb_sample_table_t* table = &sound->sample_table;
    if (!(table->config & SOUND_FIFO_IRQ_ENABLE) || table->int_pending) {
        return 0;
    }
    if (table->state != 0) {
        return SOUND_FIFO_PIPELINE_END - table->state;
    }
    if (table->hold) {
        return 0;
    }
    return (SOUND_SAMPLE_READY_CLOCK + SOUND_SAMPLE_PERIOD_CLOCKS -
            sound->sample_clock_counter) % SOUND_SAMPLE_PERIOD_CLOCKS +
            SOUND_FIFO_PIPELINE_END;
}


static void audio_callback(void* rbuf, unsigned int frames)
{
    int16_t* buffer = rbuf;
    /* The host only consumes complete frames; all registers and synthesis
     * belong to the emulation thread. */
    output_lock(g_sound);
    for (uint32_t i = 0; i < frames; i++) {
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
            if (sample_table_enabled(sound)) {
                return tbl->int_pending;
            }
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


void zvb_sound_write(zvb_sound_t* sound, uint32_t port, uint8_t value)
{
    if (!sound) {
        return;
    }
    sound->settle_clocks = SOUND_VOICE_PIPELINE_CLOCKS;
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
            if (sample_table_enabled(sound) && (value & 1)) {
                tbl->int_pending = false;
            }
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
