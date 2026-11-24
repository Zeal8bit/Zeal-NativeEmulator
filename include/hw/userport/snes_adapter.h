/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hw/device.h"
#include "hw/pio.h"


#ifdef __cplusplus
extern "C" {
#endif

struct zeal_t;

#define SNES_IO_DATA_1 0
#define SNES_IO_DATA_2 1
#define SNES_IO_LATCH  2
#define SNES_IO_CLOCK  3

#define SNES_CONTROLLER_COUNT   2
// -------------------------------------------------------------------------
// Standard controller button bit positions (16-bit serial protocol, active low)
// -------------------------------------------------------------------------
#define SNES_BTN_B      0
#define SNES_BTN_Y      1
#define SNES_BTN_SELECT 2
#define SNES_BTN_START  3
#define SNES_BTN_UP     4
#define SNES_BTN_DOWN   5
#define SNES_BTN_LEFT   6
#define SNES_BTN_RIGHT  7
#define SNES_BTN_A      8
#define SNES_BTN_X      9
#define SNES_BTN_L      10
#define SNES_BTN_R      11
// Bits 12-15 are always 1 on a standard controller; used by some accessories
#define SNES_BIT_12     12
#define SNES_BIT_13     13
#define SNES_BIT_14     14
#define SNES_BIT_15     15

// -------------------------------------------------------------------------
// Super NES Mouse (32-bit serial protocol, active low buttons)
//
// Packet 1 - first 16 serial bits:
//   Bits 0-7:   always 0
//   Bits 8-9:   buttons (right, left; 1 = pressed)
//   Bits 10-11: speed select (00=slow, 01=medium, 10=fast)
//   Bits 12-15: signature 0001
//
// Packet 2 - bits 16-31 (second latch pulse):
//   Bits 16-23: Y displacement  (bit 16 = sign: 1=up/0=down, bits 17-23 = magnitude 0-127)
//   Bits 24-31: X displacement  (bit 24 = sign: 1=left/0=right, bits 25-31 = magnitude 0-127)
// -------------------------------------------------------------------------

// Packet 1 - buttons
#define SNES_MOUSE_BTN_RIGHT    0
#define SNES_MOUSE_BTN_LEFT     1

// Packet 1 - speed select (bits 6-7, mask with (3 << SNES_MOUSE_SPEED_SHIFT))
#define SNES_MOUSE_SPEED_SHIFT  6
#define SNES_MOUSE_SPEED_SLOW   0
#define SNES_MOUSE_SPEED_MEDIUM 1
#define SNES_MOUSE_SPEED_FAST   2

// Serial bit positions (first bit clocked out is bit 0 in port_bits)
#define SNES_MOUSE_SERIAL_RIGHT       8
#define SNES_MOUSE_SERIAL_LEFT        9
#define SNES_MOUSE_SERIAL_SPEED_LSB   10
#define SNES_MOUSE_SERIAL_SPEED_MSB   11
#define SNES_MOUSE_SERIAL_SIGNATURE   15

#define SNES_MOUSE_MAG_MASK   0x7F

// Packet 2 - Y displacement (bits 16-23)
#define SNES_MOUSE_Y_SIGN_BIT   16   // 1 = moving up, 0 = moving down
#define SNES_MOUSE_Y_MAG_SHIFT  17   // 7-bit magnitude in bits 17-23

// Packet 2 - X displacement (bits 24-31)
#define SNES_MOUSE_X_SIGN_BIT   24   // 1 = moving left, 0 = moving right
#define SNES_MOUSE_X_MAG_SHIFT  25   // 7-bit magnitude in bits 25-31

/* The frontend samples input at guest LATCH. Returned packets are shifted
 * by the hardware; repeated latches may sample the same host frame. */
typedef struct {
    pio_t* pio;
    uint32_t port_bits[SNES_CONTROLLER_COUNT];
    bool attached[SNES_CONTROLLER_COUNT];
    void* userdata;
    void (*sample)(void*, uint32_t* packets, bool* attached);
    void (*trace)(void*, bool latch, const uint32_t* packets);
} snes_adapter_t;

int snes_adapter_init(snes_adapter_t*, pio_t*);

uint32_t snes_mouse_packet(uint8_t x_mag, bool x_negative, uint8_t y_mag, bool y_negative,
    uint8_t speed, bool left, bool right);

#ifdef __cplusplus
} /* extern "C" */
#endif
