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

/* Scancodes sent to the emulated keyboard, for the debugger (newest last) */
static fz_scancode_t s_log[FZ_SCANCODE_LOG];
static uint32_t s_log_total;

static void send_key(zeal_t* machine, int code, bool make, bool repeat)
{
    fifo_t* queue = &machine->keyboard.queue;
    const int wr = queue->wr;
    if (make) key_pressed(&machine->keyboard, (zeal_key_t) code);
    else key_released(&machine->keyboard, (zeal_key_t) code);

    fz_scancode_t* entry = &s_log[s_log_total % FZ_SCANCODE_LOG];
    entry->time_ns = vtimer_now_ns(&machine->cpu.clock);
    entry->key = (uint16_t) code;
    entry->make = make;
    entry->repeat = repeat;
    entry->count = 0;
    /* The bytes just pushed (popping never touches the array) */
    for (int i = wr; i != queue->wr && entry->count < FZ_SCANCODE_BYTES; i = (i + 1) % (int) queue->size) {
        entry->bytes[entry->count++] = queue->array[i];
    }
    s_log_total++;
}

int fltk_keyboard_held(uint16_t* out, int max)
{
    int n = 0;
    for (int code = 0; code < HOST_KEY_COUNT && n < max; code++) {
        if (s_keys[code].state != KEY_NOT_PRESSED) out[n++] = (uint16_t) code;
    }
    return n;
}

int fltk_keyboard_log(fz_scancode_t* out, int max)
{
    int n = 0;
    for (uint32_t i = s_log_total; i > 0 && n < max && s_log_total - i < FZ_SCANCODE_LOG; i--) {
        out[n++] = s_log[(i - 1) % FZ_SCANCODE_LOG];
    }
    return n;
}

uint32_t fltk_keyboard_log_total(void)
{
    return s_log_total;
}

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

void fz_keys_release_nonmodifiers(void)
{
    for (int code = 0; code < HOST_KEY_COUNT; code++) {
        if (key_can_repeat(code)) s_host_down[code] = false;
    }
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
        send_key(machine, code, true, false);
    }
    s_press_count = 0;

    for (int code = 0; code < HOST_KEY_COUNT; code++) {
        kb_key_t* key = &s_keys[code];
        if (key->state == KEY_NOT_PRESSED) continue;

        if (!s_host_down[code]) {
            key->state = KEY_NOT_PRESSED;
            send_key(machine, code, false, false);
            continue;
        }

        key->duration += delta;
        if (key->state == KEY_PRESSED && key_can_repeat(code) && key->duration >= start_delay) {
            send_key(machine, code, true, true);
            key->state = KEY_REPEATED;
            key->duration = 0;
        } else if (key->state == KEY_REPEATED && key->duration >= repeat_delay) {
            key->duration = 0;
            send_key(machine, code, true, true);
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
