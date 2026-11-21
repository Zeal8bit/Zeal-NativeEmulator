/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Bridge for the Hardware window's Input and Console views: PS/2 keyboard,
 * SNES adapter and UART console state as plain C data (see bridge.h). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  PS/2 keyboard                                                      */
/* ------------------------------------------------------------------ */

#define FZ_SCANCODE_LOG     64
#define FZ_SCANCODE_BYTES   8
#define FZ_KEYS_HELD_MAX    16
#define FZ_KB_FIFO_SHOWN    32

/** One key event as sent to the emulated keyboard. */
typedef struct {
    uint64_t time_ns;   /* emulated time */
    uint16_t key;       /* logical zeal_key_t */
    bool     make;      /* false: break (release) */
    bool     repeat;    /* typematic repeat */
    uint8_t  count;
    uint8_t  bytes[FZ_SCANCODE_BYTES];
} fz_scancode_t;

typedef enum {
    FZ_PS2_IDLE,
    FZ_PS2_ACTIVE,      /* start bit on the PIO pin */
    FZ_PS2_INACTIVE,    /* gap between bytes */
} fz_ps2_state_t;

typedef struct {
    int      held_count;
    uint16_t held[FZ_KEYS_HELD_MAX];    /* keys held as the machine sees them */
    int      fifo_count;                /* bytes waiting in the controller FIFO */
    uint8_t  fifo[FZ_KB_FIFO_SHOWN];    /* first bytes, next to be sent first */
    int      fifo_size;
    uint8_t  shift_register;            /* last byte presented to the CPU */
    fz_ps2_state_t state;
    bool     pin;                       /* PIO B pin level (0 = byte available) */
} fz_keyboard_state_t;

bool        fz_input_keyboard(fz_keyboard_state_t* out);
/** Most recent scancode events, newest first. Returns the count. */
int         fz_input_scancodes(fz_scancode_t* out, int max);
/** Total events since start (changes whenever a new event is logged). */
uint32_t    fz_input_scancode_total(void);
/** Short display name of a zeal_key_t ("A", "LShift", "F1"...). */
const char* fz_key_name(int zeal_key);
/** Current emulated time in nanoseconds. */
uint64_t    fz_emulated_ns(void);

/* ------------------------------------------------------------------ */
/*  SNES adapter                                                       */
/* ------------------------------------------------------------------ */

/* Button bits of fz_snes_port_t.pressed (same order as SNES_BTN_*) */
enum {
    FZ_SNES_B, FZ_SNES_Y, FZ_SNES_SELECT, FZ_SNES_START,
    FZ_SNES_UP, FZ_SNES_DOWN, FZ_SNES_LEFT, FZ_SNES_RIGHT,
    FZ_SNES_A, FZ_SNES_X, FZ_SNES_L, FZ_SNES_R,
};

typedef enum {
    FZ_SNES_NONE,
    FZ_SNES_PAD,
    FZ_SNES_MOUSE,
} fz_snes_device_t;

typedef struct {
    fz_snes_device_t device;
    int         gamepad;        /* host gamepad slot (FZ_SNES_PAD) */
    const char* name;
    bool        available;      /* host device connected */
    uint16_t    pressed;        /* live buttons, 1 = pressed (bit = FZ_SNES_*) */
    uint32_t    latched;        /* raw packet sampled at the last LATCH (active low) */
} fz_snes_port_t;

typedef struct {
    int   port;                 /* -1 when detached */
    bool  left;                 /* live host buttons */
    bool  right;
    bool  captured;
    bool  hovered;
    float scale;                /* host delta scale */
    /* Decoded from the last latched packet (valid when `latched`) */
    bool  latched;
    int   dx;
    int   dy;
    int   speed;                /* 0 slow, 1 medium, 2 fast */
    bool  latched_left;
    bool  latched_right;
} fz_snes_mouse_t;

bool     fz_input_snes_port(int port, fz_snes_port_t* out);
void     fz_input_snes_mouse(fz_snes_mouse_t* out);
uint32_t fz_input_snes_latches(void);
uint64_t fz_input_snes_last_latch_ns(void);

#ifdef __cplusplus
}
#endif
