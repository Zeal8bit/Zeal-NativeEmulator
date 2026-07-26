/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>

#if !defined(PLATFORM_WEB) && !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "raylib.h"
#include "hw/host/stdin.h"
#include "hw/keyboard.h"
#include "utils/log.h"

#if !defined(PLATFORM_WEB) && !defined(_WIN32)
#ifdef HOST_STDIN_TESTING
extern int host_stdin_test_fcntl(int fd, int command, int argument);
extern int host_stdin_test_isatty(int fd);
extern ssize_t host_stdin_test_read(int fd, void* buffer, size_t count);
extern int host_stdin_test_tcgetattr(int fd, struct termios* terminal);
extern int host_stdin_test_tcsetattr(int fd, int action, const struct termios* terminal);
#define host_fcntl     host_stdin_test_fcntl
#define host_isatty    host_stdin_test_isatty
#define host_read      host_stdin_test_read
#define host_tcgetattr host_stdin_test_tcgetattr
#define host_tcsetattr host_stdin_test_tcsetattr
#else
static int host_fcntl(int fd, int command, int argument)
{
    if(command == F_GETFL) {
        return fcntl(fd, command);
    }
    return fcntl(fd, command, argument);
}
#define host_isatty    isatty
#define host_read      read
#define host_tcgetattr tcgetattr
#define host_tcsetattr tcsetattr
#endif
#endif

typedef struct {
    uint16_t keycode;
    uint8_t modifiers;
} ascii_key_t;

static ascii_key_t ascii_to_key(uint8_t byte)
{
    if(byte >= 'a' && byte <= 'z') {
        return (ascii_key_t) { KEY_A + byte - 'a', KEYBOARD_MOD_NONE };
    }
    if(byte >= 'A' && byte <= 'Z') {
        return (ascii_key_t) { KEY_A + byte - 'A', KEYBOARD_MOD_SHIFT };
    }
    if(byte >= '0' && byte <= '9') {
        return (ascii_key_t) { KEY_ZERO + byte - '0', KEYBOARD_MOD_NONE };
    }

    switch(byte) {
        case ' ':  return (ascii_key_t) { KEY_SPACE, KEYBOARD_MOD_NONE };
        case '-':  return (ascii_key_t) { KEY_MINUS, KEYBOARD_MOD_NONE };
        case '=':  return (ascii_key_t) { KEY_EQUAL, KEYBOARD_MOD_NONE };
        case '[':  return (ascii_key_t) { KEY_LEFT_BRACKET, KEYBOARD_MOD_NONE };
        case ']':  return (ascii_key_t) { KEY_RIGHT_BRACKET, KEYBOARD_MOD_NONE };
        case '\\': return (ascii_key_t) { KEY_BACKSLASH, KEYBOARD_MOD_NONE };
        case ';':  return (ascii_key_t) { KEY_SEMICOLON, KEYBOARD_MOD_NONE };
        case '\'': return (ascii_key_t) { KEY_APOSTROPHE, KEYBOARD_MOD_NONE };
        case '`':  return (ascii_key_t) { KEY_GRAVE, KEYBOARD_MOD_NONE };
        case ',':  return (ascii_key_t) { KEY_COMMA, KEYBOARD_MOD_NONE };
        case '.':  return (ascii_key_t) { KEY_PERIOD, KEYBOARD_MOD_NONE };
        case '/':  return (ascii_key_t) { KEY_SLASH, KEYBOARD_MOD_NONE };
        case '!':  return (ascii_key_t) { KEY_ONE, KEYBOARD_MOD_SHIFT };
        case '@':  return (ascii_key_t) { KEY_TWO, KEYBOARD_MOD_SHIFT };
        case '#':  return (ascii_key_t) { KEY_THREE, KEYBOARD_MOD_SHIFT };
        case '$':  return (ascii_key_t) { KEY_FOUR, KEYBOARD_MOD_SHIFT };
        case '%':  return (ascii_key_t) { KEY_FIVE, KEYBOARD_MOD_SHIFT };
        case '^':  return (ascii_key_t) { KEY_SIX, KEYBOARD_MOD_SHIFT };
        case '&':  return (ascii_key_t) { KEY_SEVEN, KEYBOARD_MOD_SHIFT };
        case '*':  return (ascii_key_t) { KEY_EIGHT, KEYBOARD_MOD_SHIFT };
        case '(':  return (ascii_key_t) { KEY_NINE, KEYBOARD_MOD_SHIFT };
        case ')':  return (ascii_key_t) { KEY_ZERO, KEYBOARD_MOD_SHIFT };
        case '_':  return (ascii_key_t) { KEY_MINUS, KEYBOARD_MOD_SHIFT };
        case '+':  return (ascii_key_t) { KEY_EQUAL, KEYBOARD_MOD_SHIFT };
        case '{':  return (ascii_key_t) { KEY_LEFT_BRACKET, KEYBOARD_MOD_SHIFT };
        case '}':  return (ascii_key_t) { KEY_RIGHT_BRACKET, KEYBOARD_MOD_SHIFT };
        case '|':  return (ascii_key_t) { KEY_BACKSLASH, KEYBOARD_MOD_SHIFT };
        case ':':  return (ascii_key_t) { KEY_SEMICOLON, KEYBOARD_MOD_SHIFT };
        case '"':  return (ascii_key_t) { KEY_APOSTROPHE, KEYBOARD_MOD_SHIFT };
        case '~':  return (ascii_key_t) { KEY_GRAVE, KEYBOARD_MOD_SHIFT };
        case '<':  return (ascii_key_t) { KEY_COMMA, KEYBOARD_MOD_SHIFT };
        case '>':  return (ascii_key_t) { KEY_PERIOD, KEYBOARD_MOD_SHIFT };
        case '?':  return (ascii_key_t) { KEY_SLASH, KEYBOARD_MOD_SHIFT };
        default:   return (ascii_key_t) { KEY_NULL, KEYBOARD_MOD_NONE };
    }
}

void host_stdin_reset(host_stdin_t* host_stdin)
{
    if(host_stdin == NULL) {
        return;
    }
    host_stdin->pending = false;
    host_stdin->previous_was_cr = false;
    host_stdin->terminal_state = HOST_STDIN_TERMINAL_NORMAL;
    host_stdin->sequence_len = 0;
    host_stdin->replay_pos = 0;
    host_stdin->escape_elapsed = 0;
    host_stdin->replay_escape = false;
}

void host_stdin_deinit(host_stdin_t* host_stdin)
{
    if(host_stdin == NULL) {
        return;
    }
#if !defined(PLATFORM_WEB) && !defined(_WIN32)
    if(host_stdin->termios_saved) {
        if(host_tcsetattr(STDIN_FILENO, TCSANOW, &host_stdin->termios) == -1) {
            log_perror("[KEYBOARD] Failed to restore stdin terminal settings");
        } else {
            host_stdin->termios_saved = false;
        }
    }
    if(host_stdin->flags_saved) {
        if(host_fcntl(STDIN_FILENO, F_SETFL, host_stdin->flags) == -1) {
            log_perror("[KEYBOARD] Failed to restore stdin flags");
        } else {
            host_stdin->flags_saved = false;
        }
    }
#endif
    host_stdin->enabled = false;
    host_stdin_reset(host_stdin);
}

int host_stdin_init(host_stdin_t* host_stdin, bool enabled)
{
    if(host_stdin == NULL) {
        return -1;
    }

    host_stdin->pending_byte = 0;
    host_stdin->flags = 0;
    host_stdin->enabled = false;
    host_stdin->flags_saved = false;
    host_stdin->eof = false;
#if !defined(PLATFORM_WEB) && !defined(_WIN32)
    host_stdin->is_tty = false;
    host_stdin->termios_saved = false;
#endif
    host_stdin_reset(host_stdin);

#if !defined(PLATFORM_WEB) && !defined(_WIN32)
    if(enabled) {
        host_stdin->flags = host_fcntl(STDIN_FILENO, F_GETFL, 0);
        if(host_stdin->flags == -1) {
            log_perror("[KEYBOARD] Failed to get stdin flags");
            host_stdin_deinit(host_stdin);
            return -1;
        }
        host_stdin->flags_saved = true;
        if(host_fcntl(STDIN_FILENO, F_SETFL, host_stdin->flags | O_NONBLOCK) == -1) {
            log_perror("[KEYBOARD] Failed to make stdin non-blocking");
            host_stdin_deinit(host_stdin);
            return -1;
        }

        host_stdin->is_tty = host_isatty(STDIN_FILENO) != 0;
        if(host_stdin->is_tty) {
            if(host_tcgetattr(STDIN_FILENO, &host_stdin->termios) == -1) {
                log_perror("[KEYBOARD] Failed to get stdin terminal settings");
                host_stdin_deinit(host_stdin);
                return -1;
            }
            host_stdin->termios_saved = true;

            struct termios terminal = host_stdin->termios;
            terminal.c_lflag &= (tcflag_t)~(ICANON | ECHO);
            terminal.c_cc[VMIN] = 1;
            terminal.c_cc[VTIME] = 0;
            if(host_tcsetattr(STDIN_FILENO, TCSANOW, &terminal) == -1) {
                log_perror("[KEYBOARD] Failed to configure stdin terminal");
                host_stdin_deinit(host_stdin);
                return -1;
            }
        }
        host_stdin->enabled = true;
    }
#else
    (void)enabled;
#endif
    return 0;
}

bool host_stdin_type_byte(host_stdin_t* host_stdin, struct keyboard* keyboard, uint8_t byte)
{
    if(host_stdin == NULL || keyboard == NULL) {
        return false;
    }

    if(byte == '\n' && host_stdin->previous_was_cr) {
        host_stdin->previous_was_cr = false;
        return true;
    }
    host_stdin->previous_was_cr = byte == '\r';

    if(byte == 0 || byte == 0x03 || byte >= 0x80) {
        v_log_printf(2, "[KEYBOARD] Ignoring unsupported stdin byte 0x%02x\n", byte);
        return true;
    }

    switch(byte) {
        case '\b':
        case 0x7f:
            return keyboard_tap_key(keyboard, KEY_BACKSPACE, KEYBOARD_MOD_NONE);
        case '\t':
            return keyboard_tap_key(keyboard, KEY_TAB, KEYBOARD_MOD_NONE);
        case '\r':
        case '\n':
            return keyboard_tap_key(keyboard, KEY_ENTER, KEYBOARD_MOD_NONE);
        case 0x1b:
            return keyboard_tap_key(keyboard, KEY_ESCAPE, KEYBOARD_MOD_NONE);
        default:
            break;
    }

    if(byte >= 0x01 && byte <= 0x1a) {
        return keyboard_tap_key(keyboard, KEY_A + byte - 1, KEYBOARD_MOD_CTRL);
    }

    const ascii_key_t key = ascii_to_key(byte);
    if(key.keycode == KEY_NULL) {
        v_log_printf(2, "[KEYBOARD] Ignoring unsupported stdin byte 0x%02x\n", byte);
        return true;
    }
    return keyboard_tap_key(keyboard, key.keycode, key.modifiers);
}

static uint8_t terminal_modifiers(unsigned int parameter)
{
    switch(parameter) {
        case 2: return KEYBOARD_MOD_SHIFT;
        case 3: return KEYBOARD_MOD_ALT;
        case 4: return KEYBOARD_MOD_SHIFT | KEYBOARD_MOD_ALT;
        case 5: return KEYBOARD_MOD_CTRL;
        case 6: return KEYBOARD_MOD_SHIFT | KEYBOARD_MOD_CTRL;
        case 7: return KEYBOARD_MOD_ALT | KEYBOARD_MOD_CTRL;
        case 8: return KEYBOARD_MOD_SHIFT | KEYBOARD_MOD_ALT | KEYBOARD_MOD_CTRL;
        default: return KEYBOARD_MOD_NONE;
    }
}

static bool terminal_parameters(const uint8_t* sequence, uint8_t start, uint8_t end,
                                unsigned int* primary, unsigned int* modifier)
{
    unsigned int values[2] = { 0, 0 };
    uint8_t value_index = 0;
    bool have_digit = false;

    for(uint8_t i = start; i < end; i++) {
        const uint8_t byte = sequence[i];
        if(byte >= '0' && byte <= '9') {
            values[value_index] = values[value_index] * 10 + byte - '0';
            have_digit = true;
        } else if(byte == ';' && value_index == 0) {
            value_index = 1;
            have_digit = false;
        } else {
            return false;
        }
    }

    if(value_index == 1 && !have_digit) {
        return false;
    }
    *primary = values[0];
    *modifier = value_index == 0 ? 1 : values[1];
    return true;
}

static bool terminal_decode_sequence(host_stdin_t* host_stdin, uint16_t* keycode,
                                     uint8_t* modifiers)
{
    const uint8_t* sequence = host_stdin->sequence;
    const uint8_t count = host_stdin->sequence_len;
    if(count < 2) {
        return false;
    }

    const uint8_t prefix = sequence[0];
    const uint8_t final = sequence[count - 1];
    unsigned int primary = 0;
    unsigned int modifier = 1;
    if(!terminal_parameters(sequence, 1, count - 1, &primary, &modifier)) {
        return false;
    }
    *modifiers = terminal_modifiers(modifier);

    if(prefix == 'O') {
        switch(final) {
            case 'A': *keycode = KEY_UP; return primary == 0;
            case 'B': *keycode = KEY_DOWN; return primary == 0;
            case 'C': *keycode = KEY_RIGHT; return primary == 0;
            case 'D': *keycode = KEY_LEFT; return primary == 0;
            case 'H': *keycode = KEY_HOME; return primary == 0;
            case 'F': *keycode = KEY_END; return primary == 0;
            case 'P': *keycode = KEY_F1; return primary == 0;
            case 'Q': *keycode = KEY_F2; return primary == 0;
            case 'R': *keycode = KEY_F3; return primary == 0;
            case 'S': *keycode = KEY_F4; return primary == 0;
            default: return false;
        }
    }

    if(prefix != '[') {
        return false;
    }

    switch(final) {
        case 'A': *keycode = KEY_UP; return primary == 0 || primary == 1;
        case 'B': *keycode = KEY_DOWN; return primary == 0 || primary == 1;
        case 'C': *keycode = KEY_RIGHT; return primary == 0 || primary == 1;
        case 'D': *keycode = KEY_LEFT; return primary == 0 || primary == 1;
        case 'H': *keycode = KEY_HOME; return primary == 0 || primary == 1;
        case 'F': *keycode = KEY_END; return primary == 0 || primary == 1;
        case 'P': *keycode = KEY_F1; return primary == 1;
        case 'Q': *keycode = KEY_F2; return primary == 1;
        case 'R': *keycode = KEY_F3; return primary == 1;
        case 'S': *keycode = KEY_F4; return primary == 1;
        case 'Z':
            *keycode = KEY_TAB;
            *modifiers = KEYBOARD_MOD_SHIFT;
            return primary == 0;
        case '~':
            break;
        default:
            return false;
    }

    switch(primary) {
        case 1:
        case 7:  *keycode = KEY_HOME; return true;
        case 2:  *keycode = KEY_INSERT; return true;
        case 3:  *keycode = KEY_DELETE; return true;
        case 4:
        case 8:  *keycode = KEY_END; return true;
        case 5:  *keycode = KEY_PAGE_UP; return true;
        case 6:  *keycode = KEY_PAGE_DOWN; return true;
        case 11: *keycode = KEY_F1; return true;
        case 12: *keycode = KEY_F2; return true;
        case 13: *keycode = KEY_F3; return true;
        case 14: *keycode = KEY_F4; return true;
        case 15: *keycode = KEY_F5; return true;
        case 17: *keycode = KEY_F6; return true;
        case 18: *keycode = KEY_F7; return true;
        case 19: *keycode = KEY_F8; return true;
        case 20: *keycode = KEY_F9; return true;
        case 21: *keycode = KEY_F10; return true;
        case 23: *keycode = KEY_F11; return true;
        case 24: *keycode = KEY_F12; return true;
        default: return false;
    }
}

static void terminal_reset(host_stdin_t* host_stdin)
{
    host_stdin_reset(host_stdin);
}

static void terminal_begin_replay(host_stdin_t* host_stdin)
{
    host_stdin->terminal_state = HOST_STDIN_TERMINAL_REPLAY;
    host_stdin->replay_pos = 0;
    host_stdin->escape_elapsed = 0;
    host_stdin->replay_escape = true;
}

bool host_stdin_terminal_byte(host_stdin_t* host_stdin, struct keyboard* keyboard, uint8_t byte)
{
    if(host_stdin == NULL || keyboard == NULL) {
        return false;
    }

    switch(host_stdin->terminal_state) {
        case HOST_STDIN_TERMINAL_NORMAL:
            if(byte == 0x1b) {
                host_stdin->terminal_state = HOST_STDIN_TERMINAL_ESCAPE;
                host_stdin->sequence_len = 0;
                host_stdin->escape_elapsed = 0;
                return true;
            }
            return host_stdin_type_byte(host_stdin, keyboard, byte);

        case HOST_STDIN_TERMINAL_ESCAPE:
            host_stdin->escape_elapsed = 0;
            if(byte == '[' || byte == 'O') {
                host_stdin->sequence[0] = byte;
                host_stdin->sequence_len = 1;
                host_stdin->terminal_state =
                    byte == '[' ? HOST_STDIN_TERMINAL_CSI : HOST_STDIN_TERMINAL_SS3;
                return true;
            }
            if(byte >= 0x20 && byte <= 0x7e) {
                const ascii_key_t key = ascii_to_key(byte);
                if(key.keycode != KEY_NULL) {
                    if(!keyboard_tap_key(keyboard, key.keycode,
                                         key.modifiers | KEYBOARD_MOD_ALT)) {
                        return false;
                    }
                    terminal_reset(host_stdin);
                    return true;
                }
            }
            if(!keyboard_tap_key(keyboard, KEY_ESCAPE, KEYBOARD_MOD_NONE)) {
                return false;
            }
            terminal_reset(host_stdin);
            return host_stdin_type_byte(host_stdin, keyboard, byte);

        case HOST_STDIN_TERMINAL_CSI:
        case HOST_STDIN_TERMINAL_SS3: {
            host_stdin->escape_elapsed = 0;
            if(host_stdin->sequence_len == HOST_STDIN_SEQUENCE_MAX) {
                terminal_begin_replay(host_stdin);
                return false;
            }

            host_stdin->sequence[host_stdin->sequence_len++] = byte;
            if(byte >= 0x40 && byte <= 0x7e) {
                uint16_t keycode;
                uint8_t modifiers;
                if(terminal_decode_sequence(host_stdin, &keycode, &modifiers)) {
                    if(!keyboard_tap_key(keyboard, keycode, modifiers)) {
                        host_stdin->sequence_len--;
                        return false;
                    }
                    terminal_reset(host_stdin);
                    return true;
                }
                terminal_begin_replay(host_stdin);
            } else if(byte < 0x20 || byte > 0x3f) {
                terminal_begin_replay(host_stdin);
            }
            return true;
        }

        case HOST_STDIN_TERMINAL_REPLAY:
            return false;
    }
    return false;
}

static void terminal_tick(host_stdin_t* host_stdin, struct keyboard* keyboard, int elapsed)
{
    if(host_stdin->terminal_state == HOST_STDIN_TERMINAL_REPLAY) {
        if(host_stdin->replay_escape) {
            if(!keyboard_tap_key(keyboard, KEY_ESCAPE, KEYBOARD_MOD_NONE)) {
                return;
            }
            host_stdin->replay_escape = false;
            return;
        }
        if(host_stdin->replay_pos < host_stdin->sequence_len) {
            if(host_stdin_type_byte(host_stdin, keyboard,
                                    host_stdin->sequence[host_stdin->replay_pos])) {
                host_stdin->replay_pos++;
            }
            return;
        }
        terminal_reset(host_stdin);
        return;
    }

    if(host_stdin->terminal_state == HOST_STDIN_TERMINAL_ESCAPE ||
       host_stdin->terminal_state == HOST_STDIN_TERMINAL_CSI ||
       host_stdin->terminal_state == HOST_STDIN_TERMINAL_SS3) {
        host_stdin->escape_elapsed += (uint32_t)elapsed;
        if(host_stdin->escape_elapsed >= HOST_STDIN_ESCAPE_TIMEOUT) {
            terminal_begin_replay(host_stdin);
            if(keyboard_tap_key(keyboard, KEY_ESCAPE, KEYBOARD_MOD_NONE)) {
                host_stdin->replay_escape = false;
            }
        }
    }
}

void host_stdin_tick(host_stdin_t* host_stdin, struct keyboard* keyboard, int elapsed)
{
    if(host_stdin == NULL || keyboard == NULL || elapsed < 0) {
        return;
    }

    terminal_tick(host_stdin, keyboard, elapsed);

#if !defined(PLATFORM_WEB) && !defined(_WIN32)
    if(host_stdin->enabled && !host_stdin->eof && !host_stdin->pending &&
       host_stdin->terminal_state != HOST_STDIN_TERMINAL_REPLAY) {
        uint8_t byte;
        ssize_t bytes_read;
        do {
            bytes_read = host_read(STDIN_FILENO, &byte, sizeof(byte));
        } while(bytes_read < 0 && errno == EINTR);

        if(bytes_read == 1) {
            host_stdin->pending_byte = byte;
            host_stdin->pending = true;
        } else if(bytes_read == 0) {
            host_stdin->eof = true;
        } else if(errno != EAGAIN && errno != EWOULDBLOCK) {
            log_perror("[KEYBOARD] Failed to read stdin");
            host_stdin->eof = true;
        }
    }
#endif

    if(host_stdin->pending &&
       host_stdin_terminal_byte(host_stdin, keyboard, host_stdin->pending_byte)) {
        host_stdin->pending = false;
    }
}
