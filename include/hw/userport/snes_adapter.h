#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hw/device.h"
#include "hw/pio.h"

#define SNES_IO_DATA_1 0
#define SNES_IO_DATA_2 1
#define SNES_IO_LATCH  2
#define SNES_IO_CLOCK  3

#define SNES_CONTROLLER_COUNT 2

// Left thumbstick axis threshold for D-pad emulation (range -1.0 to 1.0)
#define SNES_STICK_DEADZONE 0.5f

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
// Packet 1 — bits 0-15 (same latch/clock as controller):
//   Bits 0-1:   buttons (right, left)
//   Bits 2-5:   always 0
//   Bits 6-7:   speed select (00=slow, 01=medium, 10=fast)
//   Bits 8-15:  always 0
//
// Packet 2 — bits 16-31 (second latch pulse):
//   Bits 16-23: Y displacement  (bit 16 = sign: 0=up/1=down, bits 17-23 = magnitude 0-127)
//   Bits 24-31: X displacement  (bit 24 = sign: 0=left/1=right, bits 25-31 = magnitude 0-127)
// -------------------------------------------------------------------------

// Packet 1 — buttons (active low)
#define SNES_MOUSE_BTN_RIGHT    0
#define SNES_MOUSE_BTN_LEFT     1

// Packet 1 — speed select (bits 6-7, mask with (3 << SNES_MOUSE_SPEED_SHIFT))
#define SNES_MOUSE_SPEED_SHIFT  6
#define SNES_MOUSE_SPEED_SLOW   0
#define SNES_MOUSE_SPEED_MEDIUM 1
#define SNES_MOUSE_SPEED_FAST   2

// Packet 2 — Y displacement (bits 16-23)
#define SNES_MOUSE_Y_SIGN_BIT   16   // 0 = moving up, 1 = moving down
#define SNES_MOUSE_Y_MAG_SHIFT  17   // 7-bit magnitude in bits 17-23
#define SNES_MOUSE_Y_MAG_MASK   0x7F

// Packet 2 — X displacement (bits 24-31)
#define SNES_MOUSE_X_SIGN_BIT   24   // 0 = moving left, 1 = moving right
#define SNES_MOUSE_X_MAG_SHIFT  25   // 7-bit magnitude in bits 25-31
#define SNES_MOUSE_X_MAG_MASK   0x7F

typedef struct {
    int index;      // gamepad index
    bool attached;
    uint16_t bits;  // current button bit stream
} snes_controller_t;

typedef struct {
    device_t parent;
    size_t size; // in bytes
    pio_t *pio;

    snes_controller_t controllers[SNES_CONTROLLER_COUNT];
} snes_adapter_t;

int snes_adapter_init(snes_adapter_t* snes_adapter, pio_t* pio);
void snes_adapter_attach(snes_adapter_t *snes_adapter, uint8_t index);
void snes_adapter_detatch(snes_adapter_t *snes_adapter);
void snes_adapter_latch(pio_t* pio, uint8_t pin, uint8_t bit);
void snes_adapter_clock(pio_t* pio, uint8_t pin, uint8_t bit);
