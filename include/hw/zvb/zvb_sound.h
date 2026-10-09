/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "raylib.h"

#define VOICE_COUNT      4
#define SAMPLE_RATE      44091

#define SAMPLE_MAX  65535
/* Two channels left and right */
#define SOUND_CHANNELS      2
#define SAMPLES_PER_FRAME   (735)
#define SAMPLE_FIFO_SIZE    (256)
#define SAMPLE_OUTPUT_SIZE  (4096)

// Waveform types
#define WAVE_SQUARE   0
#define WAVE_TRIANGLE 1
#define WAVE_SAWTOOTH 2
#define WAVE_NOISE    3

/**
 * @brief I/O registers address, relative to the controller
 */
#define REG_FREQ_LOW   0x0
#define REG_FREQ_HIGH  0x1
#define REG_WAVEFORM   0x2
    /* Duty cycle starts at bit 5 */
    #define REG_WAVEFORM_DUTY_SH 5
#define REG_VOICE_VOL  0x3
#define REG_MST_LEFT   0xB
#define REG_MST_RIGHT  0xC
#define REG_MST_HOLD   0xD
#define REG_MST_VOL    0xE
#define REG_MST_ENA    0xF

typedef struct {
    uint8_t freq_low;
    uint8_t freq_high;
    uint8_t wave;
    uint8_t duty;
    uint8_t voice_volume;
    bool  hold;
    /* Latch changes until the waveform can safely restart. */
    uint16_t frequency;
    uint16_t max_state;
    uint8_t wave_latch;
    uint8_t duty_latch;
    bool need_reload;
    bool decrementing;
    unsigned int phase;
    atomic_int output;
} zvb_voice_t;


typedef struct {
    bool hold;
    int divider;
    int config;
    bool is_u8;
    bool is_signed;
    /* FIFO-related */
    int fifo_head;
    int fifo_tail;
    bool fifo_empty;
    uint8_t fifo[SAMPLE_FIFO_SIZE];
    /* Baudrate divider counter, used to know when to go to the next sample in the FIFO */
    int baud_count;
    bool int_pending;
    uint8_t state;
    uint8_t ram_output;
    uint16_t sample_output;
    /* CPU clock owns the FIFO; the host audio thread only reads this sample. */
    atomic_int output;
} zvb_sample_table_t;

typedef struct {
    int16_t left;
    int16_t right;
} zvb_pcm_frame_t;

typedef struct {
    zvb_voice_t        voices[VOICE_COUNT];
    uint_fast8_t       hold_voices;
    uint_fast8_t       enabled_voices;
    uint_fast8_t       left_voices;
    uint_fast8_t       right_voices;
    uint_fast8_t       master_volume;
    zvb_sample_table_t sample_table;
    /* RayLib's audio stream */
    AudioStream        stream;
    bool               enabled;
    uint16_t           sample_clock_counter;
    uint16_t           lfsr;
    uint8_t            mix_state;
    uint16_t mean_left;
    uint16_t mean_right;
    /* Host playback queue, separate from the emulated hardware FIFO. */
    atomic_flag        output_lock;
    zvb_pcm_frame_t    output_samples[SAMPLE_OUTPUT_SIZE];
    zvb_pcm_frame_t    output_last;
    uint32_t output_head;
    uint32_t output_tail;
    uint32_t output_count;
} zvb_sound_t;


/**
 * @brief Initialize the sound controller
 */
void zvb_sound_init(zvb_sound_t* sound, bool enabled);


/**
 * @brief Simulate a hardware reset on the sound controller.
 */
void zvb_sound_reset(zvb_sound_t* sound);


/**
 * @brief Function to call when a read occurs on the sound I/O controller.
 *
 * @param port Sound register to read
 */
uint8_t zvb_sound_read(zvb_sound_t* sound, uint32_t port);


/**
 * @brief Function to call when a write occurs on the sound I/O controller.
 *
 * @param port Sound register to write
 * @param value Value of the register
 */
void zvb_sound_write(zvb_sound_t* sound, uint32_t port, uint8_t value);

/* One master clock; FIFO-empty interrupts are independent of host audio. */
void zvb_sound_clock(zvb_sound_t* sound);
/* Clocks until the next FIFO interrupt opportunity, or zero when inactive. */
uint32_t zvb_sound_clocks_until_interrupt(const zvb_sound_t* sound);
static inline bool zvb_sound_interrupt(const zvb_sound_t* sound)

{
    return (sound->sample_table.config & 8) && sound->sample_table.int_pending;
}


/**
 * @brief Deinitialize the sound controller
 */
void zvb_sound_deinit(zvb_sound_t* sound);
