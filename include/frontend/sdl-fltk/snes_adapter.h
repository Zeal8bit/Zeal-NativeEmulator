/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "hw/userport/snes_adapter.h"

#define SNES_GAMEPAD_COUNT      4
#define SNES_MOUSE_DEFAULT_PORT 1
#define SNES_PORT_DETACHED      -1

// Convert host-window mouse pixels to SNES mouse mickeys. The mouse test app divides deltas by 4.
#define SNES_MOUSE_DELTA_SCALE      1.0f
#define SNES_MOUSE_DELTA_SCALE_MIN  0.25f
#define SNES_MOUSE_DELTA_SCALE_MAX  8.0f
#define SNES_MOUSE_DELTA_SCALE_STEP 0.05f
#define SNES_MOUSE_DEFAULT_SPEED    SNES_MOUSE_SPEED_SLOW

// Thumbstick axis threshold for D-pad emulation (range -1.0 to 1.0)
#define SNES_STICK_DEADZONE 0.5f
// Trigger axis threshold for L/R emulation (range -1.0 to 1.0)
#define SNES_TRIGGER_DEADZONE 0.5f

typedef struct {
    int index;      // gamepad index
    int port;
    bool attached;
} snes_controller_t;

typedef struct {
    bool attached;
    bool captured;
    bool capture_button_down;
    struct zeal_t* machine;
    float delta_scale;
    /* Host pointer state, fed by the FLTK screen widget between latches. */
    bool hovered;
    bool button_left;
    bool button_right;
    float delta_x;
    float delta_y;
} snes_mouse_t;

typedef enum {
    SNES_PORT_DEVICE_DETACHED,
    SNES_PORT_DEVICE_CONTROLLER,
    SNES_PORT_DEVICE_MOUSE,
} snes_port_device_t;

typedef struct {
    snes_port_device_t device;
    int controller_index;
} snes_port_assignment_t;

typedef struct {
    snes_adapter_t* core;
    size_t size; // in bytes
    pio_t*          pio;

    uint16_t virtual_controller_bits;
    bool virtual_controller_enabled;
    snes_port_assignment_t ports[SNES_CONTROLLER_COUNT];
    snes_controller_t controllers[SNES_GAMEPAD_COUNT];
    /* Device seen in each slot at the last update (snes_controller_id, 0 =
     * none): a pad is assigned a port automatically only when a device
     * appears, so an explicit choice (Detached, the mouse on its port) stays
     * until that device is unplugged. A different device in the same slot,
     * even replaced between two updates, counts as a new one. */
    uint32_t seen[SNES_GAMEPAD_COUNT];
    snes_mouse_t mouse;
    /* Last packets sampled at a guest LATCH (debugger) */
    uint32_t last_packets[SNES_CONTROLLER_COUNT];
    uint32_t latch_count;
    uint64_t last_latch_ns;
} fltk_snes_t;

int fltk_snes_init(fltk_snes_t* snes_adapter, snes_adapter_t* core);
void snes_adapter_attach(fltk_snes_t* snes_adapter, uint8_t index);
void snes_adapter_detach(fltk_snes_t* snes_adapter);
void snes_adapter_update(fltk_snes_t* snes_adapter);
void snes_adapter_set_controller_port(fltk_snes_t* snes_adapter, uint8_t index, int port);
void snes_adapter_set_mouse_port(fltk_snes_t* snes_adapter, int port);
void snes_adapter_reset_mouse_scale(fltk_snes_t* snes_adapter);
int  snes_adapter_get_controller_port(const fltk_snes_t* snes_adapter, uint8_t index);
int  snes_adapter_get_mouse_port(const fltk_snes_t* snes_adapter);
void snes_adapter_set_virtual_button(fltk_snes_t* snes_adapter, uint8_t button, bool pressed);
void snes_adapter_clear_virtual_buttons(fltk_snes_t* snes_adapter);

extern fltk_snes_t frontend_snes;
