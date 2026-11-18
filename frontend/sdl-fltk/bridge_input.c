/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Machine-side implementation of bridge_input.h: keyboard and SNES adapter
 * state for the Input view, UART capture for the Console view. */

#include <stdio.h>
#include <string.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/common/host.h"
#include "hw/keys.h"

/* ------------------------------------------------------------------ */
/*  PS/2 keyboard                                                      */
/* ------------------------------------------------------------------ */

bool fz_input_keyboard(fz_keyboard_state_t* out)
{
    memset(out, 0, sizeof(*out));
    out->held_count = fltk_keyboard_held(out->held, FZ_KEYS_HELD_MAX);
    if (fltk_machine == NULL) return false;

    const keyboard_t* kb = &fltk_machine->keyboard;
    const fifo_t* q = &kb->queue;
    out->fifo_size = (int) q->size;
    if (!q->empty && q->array && q->size > 0) {
        int count = (q->wr - q->rd + (int) q->size) % (int) q->size;
        if (count == 0) count = (int) q->size;
        out->fifo_count = count;
        for (int i = 0; i < count && i < FZ_KB_FIFO_SHOWN; i++) {
            out->fifo[i] = q->array[(q->rd + i) % (int) q->size];
        }
    }
    out->shift_register = kb->shift_register;
    out->state = kb->state == PS2_ACTIVE ? FZ_PS2_ACTIVE :
                 kb->state == PS2_INACTIVE ? FZ_PS2_INACTIVE : FZ_PS2_IDLE;
    out->pin = kb->pin_state != 0;
    return true;
}

int fz_input_scancodes(fz_scancode_t* out, int max)
{
    return fltk_keyboard_log(out, max);
}

uint32_t fz_input_scancode_total(void)
{
    return fltk_keyboard_log_total();
}

uint64_t fz_emulated_ns(void)
{
    return fltk_machine ? vtimer_now_ns(&fltk_machine->cpu.clock) : 0;
}

const char* fz_key_name(int key)
{
    static char ascii[2];
    if (key > 32 && key < 127) {
        ascii[0] = (char) key;
        ascii[1] = '\0';
        return ascii;
    }
    switch (key) {
        case ZEAL_KEY_SPACE:         return "Space";
        case ZEAL_KEY_ESCAPE:        return "Esc";
        case ZEAL_KEY_ENTER:         return "Enter";
        case ZEAL_KEY_TAB:           return "Tab";
        case ZEAL_KEY_BACKSPACE:     return "Bksp";
        case ZEAL_KEY_INSERT:        return "Ins";
        case ZEAL_KEY_DELETE:        return "Del";
        case ZEAL_KEY_RIGHT:         return "Right";
        case ZEAL_KEY_LEFT:          return "Left";
        case ZEAL_KEY_DOWN:          return "Down";
        case ZEAL_KEY_UP:            return "Up";
        case ZEAL_KEY_PAGE_UP:       return "PgUp";
        case ZEAL_KEY_PAGE_DOWN:     return "PgDn";
        case ZEAL_KEY_HOME:          return "Home";
        case ZEAL_KEY_END:           return "End";
        case ZEAL_KEY_CAPS_LOCK:     return "Caps";
        case ZEAL_KEY_SCROLL_LOCK:   return "ScrLk";
        case ZEAL_KEY_NUM_LOCK:      return "NumLk";
        case ZEAL_KEY_PRINT_SCREEN:  return "PrtSc";
        case ZEAL_KEY_PAUSE:         return "Pause";
        case ZEAL_KEY_LEFT_SHIFT:    return "LShift";
        case ZEAL_KEY_LEFT_CONTROL:  return "LCtrl";
        case ZEAL_KEY_LEFT_ALT:      return "LAlt";
        case ZEAL_KEY_LEFT_SUPER:    return "LSuper";
        case ZEAL_KEY_RIGHT_SHIFT:   return "RShift";
        case ZEAL_KEY_RIGHT_CONTROL: return "RCtrl";
        case ZEAL_KEY_RIGHT_ALT:     return "RAlt";
        case ZEAL_KEY_RIGHT_SUPER:   return "RSuper";
        case ZEAL_KEY_KB_MENU:       return "Menu";
        case ZEAL_KEY_KP_DECIMAL:    return "KP.";
        case ZEAL_KEY_KP_DIVIDE:     return "KP/";
        case ZEAL_KEY_KP_MULTIPLY:   return "KP*";
        case ZEAL_KEY_KP_SUBTRACT:   return "KP-";
        case ZEAL_KEY_KP_ADD:        return "KP+";
        case ZEAL_KEY_KP_ENTER:      return "KPEnt";
        case ZEAL_KEY_KP_EQUAL:      return "KP=";
        default: break;
    }
    static char other[8];
    if (key >= ZEAL_KEY_F1 && key <= ZEAL_KEY_F12) {
        snprintf(other, sizeof(other), "F%d", key - ZEAL_KEY_F1 + 1);
        return other;
    }
    if (key >= ZEAL_KEY_KP_0 && key <= ZEAL_KEY_KP_9) {
        snprintf(other, sizeof(other), "KP%d", key - ZEAL_KEY_KP_0);
        return other;
    }
    snprintf(other, sizeof(other), "#%d", key);
    return other;
}

/* ------------------------------------------------------------------ */
/*  SNES adapter                                                       */
/* ------------------------------------------------------------------ */

_Static_assert(FZ_SNES_B == SNES_BTN_B && FZ_SNES_SELECT == SNES_BTN_SELECT &&
               FZ_SNES_UP == SNES_BTN_UP && FZ_SNES_A == SNES_BTN_A && FZ_SNES_R == SNES_BTN_R,
               "FZ_SNES_* must match SNES_BTN_*");

bool fz_input_snes_port(int port, fz_snes_port_t* out)
{
    memset(out, 0, sizeof(*out));
    out->latched = 0xFFFFFFFF;
    out->gamepad = -1;
    if (port < 0 || port >= SNES_CONTROLLER_COUNT || frontend_snes.core == NULL) return false;

    const snes_port_assignment_t* a = &frontend_snes.ports[port];
    out->latched = frontend_snes.last_packets[port];
    switch (a->device) {
        case SNES_PORT_DEVICE_CONTROLLER: {
            const int index = a->controller_index;
            uint16_t bits = 0xFFFF;
            out->device = FZ_SNES_PAD;
            out->gamepad = index;
            if (index >= 0 && index < SNES_GAMEPAD_COUNT) {
                out->available = snes_controller_available((uint8_t) index);
                out->name = snes_controller_name((uint8_t) index);
                bits = snes_controller_bits((uint8_t) index);
                if (index == 0 && frontend_snes.virtual_controller_enabled) {
                    bits &= frontend_snes.virtual_controller_bits;
                    if (!out->available) {
                        out->available = true;
                        out->name = "On-screen controller";
                    }
                }
            }
            out->pressed = (uint16_t) (~bits & 0x0FFF);
            break;
        }
        case SNES_PORT_DEVICE_MOUSE:
            out->device = FZ_SNES_MOUSE;
            out->available = true;
            out->name = "SNES Mouse";
            break;
        case SNES_PORT_DEVICE_DETACHED:
        default:
            out->device = FZ_SNES_NONE;
            break;
    }
    return true;
}

static int mouse_axis(uint32_t packet, int sign_bit, int mag_shift)
{
    int mag = 0;
    for (int bit = 0; bit < 7; bit++) {
        mag |= (int) ((packet >> (mag_shift + bit)) & 1u) << (6 - bit);
    }
    return ((packet >> sign_bit) & 1u) ? -mag : mag;
}

void fz_input_snes_mouse(fz_snes_mouse_t* out)
{
    memset(out, 0, sizeof(*out));
    out->port = SNES_PORT_DETACHED;
    if (frontend_snes.core == NULL) return;
    const snes_mouse_t* m = &frontend_snes.mouse;
    out->port = snes_adapter_get_mouse_port(&frontend_snes);
    out->left = m->button_left;
    out->right = m->button_right;
    out->captured = m->captured;
    out->hovered = m->hovered;
    out->scale = m->delta_scale;
    if (out->port < 0 || frontend_snes.latch_count == 0) return;
    out->latched = true;
    const uint32_t p = frontend_snes.last_packets[out->port];
    out->dx = mouse_axis(p, SNES_MOUSE_X_SIGN_BIT, SNES_MOUSE_X_MAG_SHIFT);
    out->dy = mouse_axis(p, SNES_MOUSE_Y_SIGN_BIT, SNES_MOUSE_Y_MAG_SHIFT);
    out->speed = (int) (((p >> SNES_MOUSE_SERIAL_SPEED_LSB) & 1u) | (((p >> SNES_MOUSE_SERIAL_SPEED_MSB) & 1u) << 1));
    out->latched_left = (p >> SNES_MOUSE_SERIAL_LEFT) & 1u;
    out->latched_right = (p >> SNES_MOUSE_SERIAL_RIGHT) & 1u;
}

uint32_t fz_input_snes_latches(void)
{
    return frontend_snes.latch_count;
}

uint64_t fz_input_snes_last_latch_ns(void)
{
    return frontend_snes.last_latch_ns;
}

/* ------------------------------------------------------------------ */
/*  UART console capture (only with the debugger UI)                   */
/* ------------------------------------------------------------------ */

#if CONFIG_ENABLE_DEBUGGER

typedef struct {
    char     text[FZ_CONSOLE_LINE_MAX];
    int      len;
    uint64_t time_ns;
} console_line_t;

static console_line_t s_lines[FZ_CONSOLE_LINES];
static uint64_t s_first;      /* oldest kept line */
static uint64_t s_cur;        /* line being written */
static bool     s_started;    /* the current line received a byte */
static uint64_t s_serial;
static uint64_t s_bytes;

static console_line_t* cur_line(void)
{
    return &s_lines[s_cur % FZ_CONSOLE_LINES];
}

static void console_newline(void)
{
    s_cur++;
    if (s_cur - s_first >= FZ_CONSOLE_LINES) s_first = s_cur - FZ_CONSOLE_LINES + 1;
    cur_line()->len = 0;
    s_started = false;
}

static void console_put(char c)
{
    console_line_t* line = cur_line();
    if (line->len >= FZ_CONSOLE_LINE_MAX) {
        console_newline();
        line = cur_line();
        line->time_ns = fz_emulated_ns();
        s_started = true;
    }
    line->text[line->len++] = c;
}

static void console_tap(void* userdata, uint8_t byte)
{
    (void) userdata;
    s_bytes++;
    s_serial++;
    if (byte == '\r') return;
    if (!s_started) {
        cur_line()->time_ns = fz_emulated_ns();
        s_started = true;
    }
    console_line_t* line = cur_line();
    switch (byte) {
        case '\n':
            console_newline();
            break;
        case '\b':
        case 0x7F:
            if (line->len > 0) line->len--;
            break;
        case '\t':
            do console_put(' '); while (cur_line()->len % 8 != 0);
            break;
        default:
            console_put((char) byte);
            break;
    }
}

void fltk_console_bind(void)
{
    frontend_uart_tap = console_tap;
    frontend_uart_tap_userdata = NULL;
}

uint64_t fz_console_serial(void)
{
    return s_serial;
}

uint64_t fz_console_first(void)
{
    return s_first;
}

int fz_console_count(void)
{
    return (int) (s_cur - s_first) + (cur_line()->len > 0 ? 1 : 0);
}

uint64_t fz_console_bytes(void)
{
    return s_bytes;
}

bool fz_console_line(uint64_t number, fz_console_line_t* out)
{
    if (number < s_first || number > s_cur) return false;
    const console_line_t* line = &s_lines[number % FZ_CONSOLE_LINES];
    out->text = line->text;
    out->len = line->len;
    out->time_ns = line->time_ns;
    return true;
}

void fz_console_clear(void)
{
    s_first = s_cur = 0;
    s_lines[0].len = 0;
    s_started = false;
    s_serial++;
}

#else

void fltk_console_bind(void)
{
}

#endif
