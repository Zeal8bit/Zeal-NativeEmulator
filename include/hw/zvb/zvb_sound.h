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
/* Since we cannot control the number of frames the audio callback
 * will request from us, we must make sure that the FIFO bigger than
 * audio callaback's `frames` parameter. */
#define SAMPLE_FIFO_SIZE    (1024)

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

static inline float volume_steps_to_float(int value, int bits)
{
    const int mask = (1 << bits) - 1;
    const float step = 1.0 / (mask + 1);
    /* Highest bit of all volume registers mark a disable sound */
    return ((value & mask) + 1) * step;
}


typedef struct {
    uint8_t freq_low;
    uint8_t freq_high;
    uint8_t wave;
    uint8_t duty;
    uint8_t voice_volume;
    bool  noise;
    bool  hold;
    /* Internal values, unrelated to the registers */
    float volume;
    unsigned int phase;
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
    atomic_int fifo_bytes;
    uint8_t fifo[SAMPLE_FIFO_SIZE];
    /* Baudrate divider counter, used to know when to go to the next sample in the FIFO */
    int baud_count;
} zvb_sample_table_t;


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
    /* Volume interpreted from the master_volume register */
    float              left_volume;
    float              right_volume;
} zvb_sound_t;


/**
 * @brief Initialize the sound controller
 */
void zvb_sound_init(zvb_sound_t* sound);


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


/**
 * @brief Deinitialize the sound controller
 */
void zvb_sound_deinit(zvb_sound_t* sound);
