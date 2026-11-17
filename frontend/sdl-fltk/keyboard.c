/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Host keyboard state, injected into the emulated PS/2 keyboard from a
 * periodic virtual timer. FLTK events only record host state; press, release
 * and the emulated typematic repeat are generated in emulated time. */

#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "hw/keys.h"
#include "utils/helpers.h"
#include "utils/vtimer.h"

/* Period, in T-states, between host keyboard polls (15ms). */
#define HOST_KEYB_CHECK_PERIOD  US_TO_TSTATES(15000)
#define HOST_KEY_COUNT          512
#define HOST_PRESS_QUEUE        64

typedef enum {
    KEY_NOT_PRESSED,
    KEY_PRESSED,
    KEY_REPEATED,
} kb_key_state_t;

typedef struct {
    kb_key_state_t state;
    int duration;
} kb_key_t;

static vtimer_node_t s_timer;
static bool s_bound;
static zeal_t* s_machine;

/* Host side: current physical state and presses not yet consumed */
static bool s_host_down[HOST_KEY_COUNT];
static uint16_t s_press_queue[HOST_PRESS_QUEUE];
static int s_press_count;

/* Emulated side */
static kb_key_t s_keys[HOST_KEY_COUNT];

static bool key_can_repeat(int code)
{
    switch (code) {
        case ZEAL_KEY_LEFT_SHIFT:  case ZEAL_KEY_LEFT_CONTROL:
        case ZEAL_KEY_LEFT_ALT:    case ZEAL_KEY_LEFT_SUPER:
        case ZEAL_KEY_RIGHT_SHIFT: case ZEAL_KEY_RIGHT_CONTROL:
        case ZEAL_KEY_RIGHT_ALT:   case ZEAL_KEY_RIGHT_SUPER:
        case ZEAL_KEY_CAPS_LOCK:   case ZEAL_KEY_NUM_LOCK:
            return false;
        default:
            return true;
    }
}

void fz_key_event(int zeal_key, bool down)
{
    if (zeal_key <= ZEAL_KEY_NULL || zeal_key >= HOST_KEY_COUNT) return;
    if (down) {
        /* Host auto-repeat is ignored: the emulated keyboard repeats. */
        if (!s_host_down[zeal_key] && s_press_count < HOST_PRESS_QUEUE) {
            s_press_queue[s_press_count++] = (uint16_t) zeal_key;
        }
        s_host_down[zeal_key] = true;
    } else {
        s_host_down[zeal_key] = false;
    }
}

void fz_keys_release_all(void)
{
    memset(s_host_down, 0, sizeof(s_host_down));
    s_press_count = 0;
}

void fltk_keyboard_reset(void)
{
    fz_keys_release_all();
    memset(s_keys, 0, sizeof(s_keys));
}

static void keyboard_read(zeal_t* machine, int delta)
{
    /* The initial delay is ~500ms before repeat starts, then every 50ms */
    const int start_delay = us_to_tstates(500000);
    const int repeat_delay = us_to_tstates(50000);

    for (int i = 0; i < s_press_count; i++) {
        const int code = s_press_queue[i];
        s_keys[code].state = KEY_PRESSED;
        s_keys[code].duration = 0;
        key_pressed(&machine->keyboard, (zeal_key_t) code);
    }
    s_press_count = 0;

    for (int code = 0; code < HOST_KEY_COUNT; code++) {
        kb_key_t* key = &s_keys[code];
        if (key->state == KEY_NOT_PRESSED) continue;

        if (!s_host_down[code]) {
            key->state = KEY_NOT_PRESSED;
            key_released(&machine->keyboard, (zeal_key_t) code);
            continue;
        }

        key->duration += delta;
        if (key->state == KEY_PRESSED && key_can_repeat(code) && key->duration >= start_delay) {
            key_pressed(&machine->keyboard, (zeal_key_t) code);
            key->state = KEY_REPEATED;
            key->duration = 0;
        } else if (key->state == KEY_REPEATED && key->duration >= repeat_delay) {
            key->duration = 0;
            key_pressed(&machine->keyboard, (zeal_key_t) code);
        }
    }
}

static void keyboard_check_cb(void* userdata)
{
    zeal_t* machine = (zeal_t*) userdata;
    vtimer_schedule_tstates(&s_timer, HOST_KEYB_CHECK_PERIOD);
#if CONFIG_ENABLE_DEBUGGER
    if (machine->dbg_enabled && machine->dbg_state != ST_RUNNING) return;
#endif
    if (!ui_screen_focused()) {
        /* Keys typed into debugger widgets never reach the machine. */
        s_press_count = 0;
    }
    keyboard_read(machine, HOST_KEYB_CHECK_PERIOD);
}

void fltk_keyboard_bind(zeal_t* machine)
{
    s_machine = machine;
    vtimer_init_node(&s_timer, &machine->cpu.clock, keyboard_check_cb, machine);
    vtimer_schedule_tstates(&s_timer, HOST_KEYB_CHECK_PERIOD);
    s_bound = true;
}

void fltk_keyboard_unbind(void)
{
    if (!s_bound) return;
    vtimer_cancel(&s_timer);
    s_bound = false;
    s_machine = NULL;
}
