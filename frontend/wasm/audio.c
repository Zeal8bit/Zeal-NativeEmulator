/* SPDX-License-Identifier: Apache-2.0 */
#include <string.h>
#include "frontend/wasm/audio.h"

/* About 93 ms of queued audio absorbs video-frame bursts and host jitter,
 * including an occasional missed presentation while correcting clock drift.
 * Slowly correct clock drift at the host boundary, without skipping chunks
 * of the FPGA output or changing any emulated clocks/registers. */
#define SOUND_PLAYBACK_TARGET (SAMPLE_OUTPUT_SIZE / 2)
#define SOUND_PLAYBACK_CORRECTION_SECS (0.5)
#define SOUND_PLAYBACK_SMOOTH_SECS (0.25)
/* Frame-paced browser delivery supplies samples; correct residual clock drift. */
#define SOUND_PLAYBACK_MAX_CORRECTION (0.01)
/* First-order AC coupling at 5 Hz preserves bass while rejecting DC. */
#define SOUND_PLAYBACK_DC_POLE (1.0 / (1.0 + 2.0 * 3.141592653589793 * 5.0 / SAMPLE_RATE))

static wasm_audio_t playback;
static wasm_audio_t* g_sound = &playback;

static void output_lock(wasm_audio_t* sound)
{
    while (atomic_flag_test_and_set_explicit(&sound->output_lock, memory_order_acquire)) {
    }
}

static void output_unlock(wasm_audio_t* sound)
{
    atomic_flag_clear_explicit(&sound->output_lock, memory_order_release);
}

static void playback_reset(void* userdata)
{
    wasm_audio_t* sound = userdata;
    output_lock(sound);
    sound->output_count = 0;
    sound->output_tail = 0;
    sound->output_head = 0;
    sound->playback_last = (zvb_pcm_frame_t){0};
    sound->playback_started = false;
    sound->playback_position = 0;
    sound->playback_rate = 1.0;
    memset(sound->playback_dc_input, 0, sizeof(sound->playback_dc_input));
    memset(sound->playback_dc_output, 0, sizeof(sound->playback_dc_output));
    sound->playback_dc_ready = false;
    output_unlock(sound);
}

static void playback_push(void* userdata, zvb_pcm_frame_t frame)
{
    wasm_audio_t* sound = userdata;
    output_lock(sound);
    if (sound->enabled && sound->output_count < SAMPLE_OUTPUT_SIZE) {
        sound->output_samples[sound->output_head] = frame;
        sound->output_head = (sound->output_head + 1) % SAMPLE_OUTPUT_SIZE;
        sound->output_count++;
    }
    output_unlock(sound);
}

/* Keep FPGA mixer levels intact in the queue. Only the host DAC
 * needs AC coupling: a muted mixer and a held underrun sample must settle
 * to zero, and changing route/volume/duty must not leave a lasting bias. */
static zvb_pcm_frame_t playback_ac_couple(wasm_audio_t* sound, zvb_pcm_frame_t sample)
{
    const double input[SOUND_CHANNELS] = {sample.left, sample.right};
    int16_t output[SOUND_CHANNELS];
    for (unsigned channel = 0; channel < SOUND_CHANNELS; channel++) {
        if (!sound->playback_dc_ready) {
            /* Start at the first input's DC level, without a startup step. */
            sound->playback_dc_input[channel] = input[channel];
        }
        const double filtered = SOUND_PLAYBACK_DC_POLE *
                                (sound->playback_dc_output[channel] + input[channel] - sound->playback_dc_input[channel]);
        sound->playback_dc_input[channel] = input[channel];
        sound->playback_dc_output[channel] = filtered;
        /* A full-range input step can exceed signed PCM after DC removal.
         * Retain filter precision/state, clipping only the DAC sample. */
        output[channel] = filtered > INT16_MAX ? INT16_MAX : filtered < INT16_MIN ? INT16_MIN
                                                                                  : (int16_t) filtered;
    }
    sound->playback_dc_ready = true;
    return (zvb_pcm_frame_t){output[0], output[1]};
}

static void audio_callback(void* rbuf, unsigned int frames)
{
    int16_t* buffer = rbuf;
    /* The host only consumes complete frames; all registers and synthesis
     * belong to the emulation thread. */
    output_lock(g_sound);
    if (!g_sound->playback_started && g_sound->output_count >= SOUND_PLAYBACK_TARGET) {
        g_sound->playback_started = true;
    }
    if (g_sound->playback_started) {
        double correction = ((double) g_sound->output_count - SOUND_PLAYBACK_TARGET) /
                            (SAMPLE_RATE * SOUND_PLAYBACK_CORRECTION_SECS);
        if (correction > SOUND_PLAYBACK_MAX_CORRECTION) {
            correction = SOUND_PLAYBACK_MAX_CORRECTION;
        } else if (correction < -SOUND_PLAYBACK_MAX_CORRECTION) {
            correction = -SOUND_PLAYBACK_MAX_CORRECTION;
        }
        const double smoothing = frames / (SAMPLE_RATE * SOUND_PLAYBACK_SMOOTH_SECS + frames);
        g_sound->playback_rate += smoothing * (1.0 + correction - g_sound->playback_rate);
    }
    for (uint32_t i = 0; i < frames; i++) {
        zvb_pcm_frame_t sample = g_sound->playback_last;
        if (g_sound->playback_started && g_sound->output_count >= 2) {
            const zvb_pcm_frame_t first = g_sound->output_samples[g_sound->output_tail];
            const zvb_pcm_frame_t second = g_sound->output_samples[(g_sound->output_tail + 1) % SAMPLE_OUTPUT_SIZE];
            const double fraction = g_sound->playback_position;
            sample.left = first.left + (second.left - first.left) * fraction;
            sample.right = first.right + (second.right - first.right) * fraction;
            g_sound->playback_last = sample;
            g_sound->playback_position += g_sound->playback_rate;
            const uint32_t consumed = (uint32_t) g_sound->playback_position;
            g_sound->playback_position -= consumed;
            g_sound->output_tail = (g_sound->output_tail + consumed) % SAMPLE_OUTPUT_SIZE;
            g_sound->output_count -= consumed;
        } else if (g_sound->playback_started) {
            /* A long stall/pause needs a fresh reserve. Hold the last played
             * sample, not the producer's newest (possibly dropped) sample. */
            g_sound->playback_started = false;
            g_sound->playback_position = 0;
            g_sound->playback_rate = 1.0;
        }
        if (g_sound->playback_dc_ready || g_sound->playback_started) {
            sample = playback_ac_couple(g_sound, sample);
        }
        buffer[i * SOUND_CHANNELS] = sample.left;
        buffer[i * SOUND_CHANNELS + 1] = sample.right;
    }
    output_unlock(g_sound);
}

/* One playback device per graphical application; close it before rebinding. */
void wasm_audio_init(zvb_sound_t* sound, bool enabled)
{
    memset(&playback, 0, sizeof(playback));
    atomic_flag_clear(&playback.output_lock);
    playback.playback_rate = 1.0;
    playback.enabled = enabled;
    if (!enabled) return;
    InitAudioDevice();
    SetMasterVolume(1.0f);
    playback.stream = LoadAudioStream(SAMPLE_RATE, 16, SOUND_CHANNELS);
    sound->output_userdata = &playback;
    sound->output = playback_push;
    sound->output_reset = playback_reset;
    SetAudioStreamCallback(playback.stream, audio_callback);
    PlayAudioStream(playback.stream);
}

void wasm_audio_deinit(zvb_sound_t* sound)
{
    sound->output = NULL;
    sound->output_reset = NULL;
    sound->output_userdata = NULL;
    if (!playback.enabled) return;
    StopAudioStream(playback.stream);
    UnloadAudioStream(playback.stream);
    CloseAudioDevice();
    playback.enabled = false;
}
