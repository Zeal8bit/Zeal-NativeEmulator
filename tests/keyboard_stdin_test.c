/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "raylib.h"
#include "hw/keyboard.h"
#include "hw/pio.h"
#include "utils/config.h"

config_t config = { 0 };

typedef enum {
    HOST_FAIL_NONE,
    HOST_FAIL_GET_FLAGS,
    HOST_FAIL_SET_NONBLOCK,
    HOST_FAIL_GET_TERMIOS,
    HOST_FAIL_SET_TERMIOS,
} host_failure_t;

static host_failure_t host_failure;
static bool host_failure_triggered;
static unsigned int host_fcntl_calls;
static unsigned int host_isatty_calls;
static unsigned int host_read_calls;
static unsigned int host_tcgetattr_calls;
static unsigned int host_tcsetattr_calls;

static void reset_host_calls(host_failure_t failure)
{
    host_failure = failure;
    host_failure_triggered = false;
    host_fcntl_calls = 0;
    host_isatty_calls = 0;
    host_read_calls = 0;
    host_tcgetattr_calls = 0;
    host_tcsetattr_calls = 0;
}

int host_stdin_test_fcntl(int fd, int command, int argument)
{
    host_fcntl_calls++;
    if(!host_failure_triggered &&
       ((host_failure == HOST_FAIL_GET_FLAGS && command == F_GETFL) ||
        (host_failure == HOST_FAIL_SET_NONBLOCK && command == F_SETFL))) {
        host_failure_triggered = true;
        errno = EIO;
        return -1;
    }

    if(command == F_GETFL) {
        return fcntl(fd, command);
    }
    return fcntl(fd, command, argument);
}

int host_stdin_test_isatty(int fd)
{
    host_isatty_calls++;
    return isatty(fd);
}

ssize_t host_stdin_test_read(int fd, void* buffer, size_t count)
{
    host_read_calls++;
    return read(fd, buffer, count);
}

int host_stdin_test_tcgetattr(int fd, struct termios* terminal)
{
    host_tcgetattr_calls++;
    if(!host_failure_triggered && host_failure == HOST_FAIL_GET_TERMIOS) {
        host_failure_triggered = true;
        errno = EIO;
        return -1;
    }
    return tcgetattr(fd, terminal);
}

int host_stdin_test_tcsetattr(int fd, int action, const struct termios* terminal)
{
    host_tcsetattr_calls++;
    if(!host_failure_triggered && host_failure == HOST_FAIL_SET_TERMIOS) {
        host_failure_triggered = true;
        errno = EIO;
        return -1;
    }
    return tcsetattr(fd, action, terminal);
}

void pio_set_b_pin(pio_t* pio, uint8_t pin, uint8_t value)
{
    (void)pio;
    assert(pin == IO_KEYBOARD_PIN);
    assert(value <= 1);
}

static void assert_termios_equal(const struct termios* actual, const struct termios* expected)
{
    assert(actual->c_iflag == expected->c_iflag);
    assert(actual->c_oflag == expected->c_oflag);
    assert(actual->c_cflag == expected->c_cflag);
#ifdef PENDIN
    assert((actual->c_lflag & (tcflag_t)~PENDIN) ==
           (expected->c_lflag & (tcflag_t)~PENDIN));
#else
    assert(actual->c_lflag == expected->c_lflag);
#endif
    assert(memcmp(actual->c_cc, expected->c_cc, sizeof(actual->c_cc)) == 0);
}

static int replace_stdin_with_pty(int* stdin_copy, struct termios* terminal, int* flags)
{
    *stdin_copy = dup(STDIN_FILENO);
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    assert(*stdin_copy >= 0);
    assert(master >= 0);
    assert(grantpt(master) == 0);
    assert(unlockpt(master) == 0);

    char* slave_name = ptsname(master);
    assert(slave_name != NULL);
    int slave = open(slave_name, O_RDWR | O_NOCTTY);
    assert(slave >= 0);
    assert(dup2(slave, STDIN_FILENO) == STDIN_FILENO);
    close(slave);

    *flags = fcntl(STDIN_FILENO, F_GETFL);
    assert(*flags >= 0);
    assert(tcgetattr(STDIN_FILENO, terminal) == 0);
    return master;
}

static void restore_stdin(int stdin_copy, int input_fd)
{
    assert(dup2(stdin_copy, STDIN_FILENO) == STDIN_FILENO);
    close(stdin_copy);
    if(input_fd >= 0) {
        close(input_fd);
    }
}

static void test_disabled_stdin_uses_no_host_calls(void)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    reset_host_calls(HOST_FAIL_NONE);

    assert(keyboard_init(&keyboard, &pio, false) == 0);
    keyboard_tick(&keyboard, &pio, 100);
    keyboard_deinit(&keyboard);

    assert(host_fcntl_calls == 0);
    assert(host_isatty_calls == 0);
    assert(host_read_calls == 0);
    assert(host_tcgetattr_calls == 0);
    assert(host_tcsetattr_calls == 0);
}

static void test_pipe_polling_and_backpressure(void)
{
    static const uint8_t input[] = { 0x00, 0x80, 'a' };
    int stdin_copy = dup(STDIN_FILENO);
    int input_pipe[2];
    assert(stdin_copy >= 0);
    assert(pipe(input_pipe) == 0);
    assert(dup2(input_pipe[0], STDIN_FILENO) == STDIN_FILENO);
    close(input_pipe[0]);

    const int original_flags = fcntl(STDIN_FILENO, F_GETFL);
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    reset_host_calls(HOST_FAIL_NONE);
    assert(keyboard_init(&keyboard, &pio, true) == 0);
    assert((fcntl(STDIN_FILENO, F_GETFL) & O_NONBLOCK) != 0);

    keyboard_tick(&keyboard, &pio, 100);
    assert(!keyboard.host_stdin.pending);
    assert(!keyboard.host_stdin.eof);

    assert(write(input_pipe[1], input, sizeof(input)) == (ssize_t)sizeof(input));
    close(input_pipe[1]);
    input_pipe[1] = -1;
    keyboard_tick(&keyboard, &pio, 100);
    assert(!keyboard.host_stdin.pending);
    keyboard_tick(&keyboard, &pio, 100);
    assert(!keyboard.host_stdin.pending);
    keyboard_tick(&keyboard, &pio, 100);
    assert(keyboard.shift_register == 0x1c);
    keyboard_tick(&keyboard, &pio, 100);
    assert(keyboard.host_stdin.eof);

    keyboard_deinit(&keyboard);
    assert(fcntl(STDIN_FILENO, F_GETFL) == original_flags);
    restore_stdin(stdin_copy, input_pipe[1]);
}

static void test_interactive_tty(void)
{
    int stdin_copy;
    int original_flags;
    struct termios original_termios;
    int master = replace_stdin_with_pty(&stdin_copy, &original_termios, &original_flags);

    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    reset_host_calls(HOST_FAIL_NONE);
    assert(keyboard_init(&keyboard, &pio, true) == 0);
    assert(keyboard.host_stdin.is_tty);

    struct termios active;
    assert(tcgetattr(STDIN_FILENO, &active) == 0);
    assert((active.c_lflag & ICANON) == 0);
    assert((active.c_lflag & ECHO) == 0);
    assert((active.c_lflag & ISIG) == (original_termios.c_lflag & ISIG));

    const uint8_t byte = 'Z';
    assert(write(master, &byte, sizeof(byte)) == (ssize_t)sizeof(byte));
    keyboard_tick(&keyboard, &pio, 0);
    assert(!keyboard.host_stdin.pending);
    assert(keyboard.shift_register == 0x12);
    assert(fifo_size(&keyboard.queue) == 5);

    keyboard_deinit(&keyboard);
    keyboard_deinit(&keyboard);
    struct termios restored;
    assert(tcgetattr(STDIN_FILENO, &restored) == 0);
    assert_termios_equal(&restored, &original_termios);
    assert(fcntl(STDIN_FILENO, F_GETFL) == original_flags);
    restore_stdin(stdin_copy, master);
}

static void test_initialization_failure_rollback(void)
{
    static const host_failure_t failures[] = {
        HOST_FAIL_GET_FLAGS,
        HOST_FAIL_SET_NONBLOCK,
        HOST_FAIL_GET_TERMIOS,
        HOST_FAIL_SET_TERMIOS,
    };

    for(size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        int stdin_copy;
        int original_flags;
        struct termios original_termios;
        int master = replace_stdin_with_pty(&stdin_copy, &original_termios, &original_flags);

        keyboard_t keyboard = { 0 };
        pio_t pio = { 0 };
        reset_host_calls(failures[i]);
        assert(keyboard_init(&keyboard, &pio, true) == -1);
        assert(host_failure_triggered);
        assert(!keyboard.host_stdin.enabled);
        assert(!keyboard.host_stdin.flags_saved);
        assert(!keyboard.host_stdin.termios_saved);
        assert(keyboard.queue.array == NULL);
        assert(fcntl(STDIN_FILENO, F_GETFL) == original_flags);

        struct termios restored;
        assert(tcgetattr(STDIN_FILENO, &restored) == 0);
        assert_termios_equal(&restored, &original_termios);
        keyboard_deinit(&keyboard);
        restore_stdin(stdin_copy, master);
    }
}

static void assert_queue(keyboard_t* keyboard, const uint8_t* expected, size_t count)
{
    assert(fifo_size(&keyboard->queue) == count);
    for(size_t i = 0; i < count; i++) {
        uint8_t actual;
        assert(fifo_pop(&keyboard->queue, &actual));
        assert(actual == expected[i]);
    }
}

static void assert_typed(uint8_t byte, uint8_t scan, uint8_t modifiers)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    assert(keyboard_init(&keyboard, &pio, false) == 0);
    assert(host_stdin_type_byte(&keyboard.host_stdin, &keyboard, byte));

    uint8_t expected[16];
    size_t count = 0;
    if(modifiers & KEYBOARD_MOD_CTRL) {
        expected[count++] = 0xe0;
        expected[count++] = 0x14;
    }
    if(modifiers & KEYBOARD_MOD_ALT) {
        expected[count++] = 0x11;
    }
    if(modifiers & KEYBOARD_MOD_SHIFT) {
        expected[count++] = 0x12;
    }
    expected[count++] = scan;
    expected[count++] = 0xf0;
    expected[count++] = scan;
    if(modifiers & KEYBOARD_MOD_SHIFT) {
        expected[count++] = 0xf0;
        expected[count++] = 0x12;
    }
    if(modifiers & KEYBOARD_MOD_ALT) {
        expected[count++] = 0xf0;
        expected[count++] = 0x11;
    }
    if(modifiers & KEYBOARD_MOD_CTRL) {
        expected[count++] = 0xe0;
        expected[count++] = 0xf0;
        expected[count++] = 0x14;
    }

    assert_queue(&keyboard, expected, count);
    keyboard_deinit(&keyboard);
}

static void test_ascii_translation(void)
{
    static const uint8_t letter_scans[] = {
        0x1c, 0x32, 0x21, 0x23, 0x24, 0x2b, 0x34, 0x33, 0x43, 0x3b,
        0x42, 0x4b, 0x3a, 0x31, 0x44, 0x4d, 0x15, 0x2d, 0x1b, 0x2c,
        0x3c, 0x2a, 0x1d, 0x22, 0x35, 0x1a,
    };
    static const uint8_t digit_scans[] = {
        0x45, 0x16, 0x1e, 0x26, 0x25, 0x2e, 0x36, 0x3d, 0x3e, 0x46,
    };
    static const struct {
        uint8_t byte;
        uint8_t scan;
        uint8_t modifiers;
    } punctuation[] = {
        { ' ', 0x29, 0 }, { '-', 0x4e, 0 }, { '=', 0x55, 0 },
        { '[', 0x54, 0 }, { ']', 0x5b, 0 }, { '\\', 0x5d, 0 },
        { ';', 0x4c, 0 }, { '\'', 0x52, 0 }, { '`', 0x0e, 0 },
        { ',', 0x41, 0 }, { '.', 0x49, 0 }, { '/', 0x4a, 0 },
        { '!', 0x16, 1 }, { '@', 0x1e, 1 }, { '#', 0x26, 1 },
        { '$', 0x25, 1 }, { '%', 0x2e, 1 }, { '^', 0x36, 1 },
        { '&', 0x3d, 1 }, { '*', 0x3e, 1 }, { '(', 0x46, 1 },
        { ')', 0x45, 1 }, { '_', 0x4e, 1 }, { '+', 0x55, 1 },
        { '{', 0x54, 1 }, { '}', 0x5b, 1 }, { '|', 0x5d, 1 },
        { ':', 0x4c, 1 }, { '"', 0x52, 1 }, { '~', 0x0e, 1 },
        { '<', 0x41, 1 }, { '>', 0x49, 1 }, { '?', 0x4a, 1 },
    };

    for(uint8_t i = 0; i < 26; i++) {
        assert_typed('a' + i, letter_scans[i], KEYBOARD_MOD_NONE);
        assert_typed('A' + i, letter_scans[i], KEYBOARD_MOD_SHIFT);
        const uint8_t control = i + 1;
        if(control != 0x03 && control != '\b' && control != '\t' &&
           control != '\n' && control != '\r') {
            assert_typed(i + 1, letter_scans[i], KEYBOARD_MOD_CTRL);
        }
    }
    for(uint8_t i = 0; i < 10; i++) {
        assert_typed('0' + i, digit_scans[i], KEYBOARD_MOD_NONE);
    }
    for(size_t i = 0; i < sizeof(punctuation) / sizeof(punctuation[0]); i++) {
        assert_typed(punctuation[i].byte, punctuation[i].scan, punctuation[i].modifiers);
    }

    assert_typed('\b', 0x66, 0);
    assert_typed(0x7f, 0x66, 0);
    assert_typed('\t', 0x0d, 0);
    assert_typed('\r', 0x5a, 0);
    assert_typed('\n', 0x5a, 0);
    assert_typed(0x1b, 0x76, 0);
}

static void test_crlf_and_unsupported_bytes(void)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    assert(keyboard_init(&keyboard, &pio, false) == 0);
    assert(host_stdin_type_byte(&keyboard.host_stdin, &keyboard, '\r'));
    assert(fifo_size(&keyboard.queue) == 3);
    assert(host_stdin_type_byte(&keyboard.host_stdin, &keyboard, '\n'));
    assert(fifo_size(&keyboard.queue) == 3);
    fifo_reset(&keyboard.queue);

    assert(host_stdin_type_byte(&keyboard.host_stdin, &keyboard, 0x00));
    assert(host_stdin_type_byte(&keyboard.host_stdin, &keyboard, 0x03));
    assert(host_stdin_type_byte(&keyboard.host_stdin, &keyboard, 0x80));
    assert(fifo_size(&keyboard.queue) == 0);
    keyboard_deinit(&keyboard);
}

static void test_atomic_queue_backpressure(void)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    assert(keyboard_init(&keyboard, &pio, false) == 0);
    for(size_t i = 0; i < FIFO_SIZE - 5; i++) {
        assert(fifo_push(&keyboard.queue, 0xaa));
    }

    assert(!host_stdin_type_byte(&keyboard.host_stdin, &keyboard, 'A'));
    assert(fifo_size(&keyboard.queue) == FIFO_SIZE - 5);

    keyboard.host_stdin.pending = true;
    keyboard.host_stdin.pending_byte = 'A';
    keyboard_tick(&keyboard, &pio, 0);
    assert(keyboard.host_stdin.pending);
    keyboard_deinit(&keyboard);
}

static void test_window_and_stdin_share_fifo(void)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    assert(keyboard_init(&keyboard, &pio, false) == 0);

    /* Raylib window input uses key_pressed(); stdin uses the terminal
     * translator. Both must preserve order in the keyboard's one PS/2 FIFO. */
    assert(key_pressed(&keyboard, KEY_A) == 0);
    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, 'b'));

    const uint8_t expected[] = {
        0x1c,
        0x32, 0xf0, 0x32,
    };
    assert_queue(&keyboard, expected, sizeof(expected));
    keyboard_deinit(&keyboard);
}

static void append_expected_key(uint8_t* expected, size_t* count, uint8_t scan,
                                bool extended, uint8_t modifiers)
{
    if(modifiers & KEYBOARD_MOD_CTRL) {
        expected[(*count)++] = 0xe0;
        expected[(*count)++] = 0x14;
    }
    if(modifiers & KEYBOARD_MOD_ALT) {
        expected[(*count)++] = 0x11;
    }
    if(modifiers & KEYBOARD_MOD_SHIFT) {
        expected[(*count)++] = 0x12;
    }
    if(extended) {
        expected[(*count)++] = 0xe0;
    }
    expected[(*count)++] = scan;
    if(extended) {
        expected[(*count)++] = 0xe0;
    }
    expected[(*count)++] = 0xf0;
    expected[(*count)++] = scan;
    if(modifiers & KEYBOARD_MOD_SHIFT) {
        expected[(*count)++] = 0xf0;
        expected[(*count)++] = 0x12;
    }
    if(modifiers & KEYBOARD_MOD_ALT) {
        expected[(*count)++] = 0xf0;
        expected[(*count)++] = 0x11;
    }
    if(modifiers & KEYBOARD_MOD_CTRL) {
        expected[(*count)++] = 0xe0;
        expected[(*count)++] = 0xf0;
        expected[(*count)++] = 0x14;
    }
}

static void assert_terminal_key(const char* sequence, uint8_t scan, bool extended,
                                uint8_t modifiers)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    assert(keyboard_init(&keyboard, &pio, false) == 0);

    for(size_t i = 0; sequence[i] != '\0'; i++) {
        assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, (uint8_t)sequence[i]));
    }
    assert(keyboard.host_stdin.terminal_state == HOST_STDIN_TERMINAL_NORMAL);

    uint8_t expected[20];
    size_t count = 0;
    append_expected_key(expected, &count, scan, extended, modifiers);
    assert_queue(&keyboard, expected, count);
    keyboard_deinit(&keyboard);
}

static void test_terminal_navigation_and_functions(void)
{
    static const struct {
        const char* sequence;
        uint8_t scan;
        bool extended;
    } keys[] = {
        { "\x1b[A", 0x75, true }, { "\x1b[B", 0x72, true },
        { "\x1b[C", 0x74, true }, { "\x1b[D", 0x6b, true },
        { "\x1b[H", 0x6c, true }, { "\x1b[F", 0x69, true },
        { "\x1b[1~", 0x6c, true }, { "\x1b[7~", 0x6c, true },
        { "\x1b[4~", 0x69, true }, { "\x1b[8~", 0x69, true },
        { "\x1b[2~", 0x70, true }, { "\x1b[3~", 0x71, true },
        { "\x1b[5~", 0x7d, true }, { "\x1b[6~", 0x7a, true },
        { "\x1bOA", 0x75, true }, { "\x1bOB", 0x72, true },
        { "\x1bOC", 0x74, true }, { "\x1bOD", 0x6b, true },
        { "\x1bOH", 0x6c, true }, { "\x1bOF", 0x69, true },
        { "\x1bOP", 0x05, false }, { "\x1bOQ", 0x06, false },
        { "\x1bOR", 0x04, false }, { "\x1bOS", 0x0c, false },
        { "\x1b[11~", 0x05, false }, { "\x1b[12~", 0x06, false },
        { "\x1b[13~", 0x04, false }, { "\x1b[14~", 0x0c, false },
        { "\x1b[15~", 0x03, false }, { "\x1b[17~", 0x0b, false },
        { "\x1b[18~", 0x83, false }, { "\x1b[19~", 0x0a, false },
        { "\x1b[20~", 0x01, false }, { "\x1b[21~", 0x09, false },
        { "\x1b[23~", 0x78, false }, { "\x1b[24~", 0x07, false },
    };

    for(size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        assert_terminal_key(keys[i].sequence, keys[i].scan, keys[i].extended,
                            KEYBOARD_MOD_NONE);
    }
    assert_terminal_key("\x1b[Z", 0x0d, false, KEYBOARD_MOD_SHIFT);
    assert_terminal_key("\x1bx", 0x22, false, KEYBOARD_MOD_ALT);
    assert_terminal_key("\x1bX", 0x22, false,
                        KEYBOARD_MOD_ALT | KEYBOARD_MOD_SHIFT);
}

static void test_terminal_modifiers(void)
{
    static const uint8_t expected_modifiers[] = {
        KEYBOARD_MOD_SHIFT,
        KEYBOARD_MOD_ALT,
        KEYBOARD_MOD_SHIFT | KEYBOARD_MOD_ALT,
        KEYBOARD_MOD_CTRL,
        KEYBOARD_MOD_SHIFT | KEYBOARD_MOD_CTRL,
        KEYBOARD_MOD_ALT | KEYBOARD_MOD_CTRL,
        KEYBOARD_MOD_SHIFT | KEYBOARD_MOD_ALT | KEYBOARD_MOD_CTRL,
    };
    char sequence[] = "\x1b[1;2A";

    for(uint8_t parameter = 2; parameter <= 8; parameter++) {
        sequence[4] = '0' + parameter;
        assert_terminal_key(sequence, 0x75, true,
                            expected_modifiers[parameter - 2]);
    }
    assert_terminal_key("\x1b[3;5~", 0x71, true, KEYBOARD_MOD_CTRL);
    assert_terminal_key("\x1b[1;6P", 0x05, false,
                        KEYBOARD_MOD_SHIFT | KEYBOARD_MOD_CTRL);
}

static void test_terminal_escape_timeout_and_replay(void)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    assert(keyboard_init(&keyboard, &pio, false) == 0);

    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, 0x1b));
    host_stdin_tick(&keyboard.host_stdin, &keyboard, HOST_STDIN_ESCAPE_TIMEOUT - 1);
    assert(fifo_size(&keyboard.queue) == 0);
    host_stdin_tick(&keyboard.host_stdin, &keyboard, 1);
    const uint8_t escape[] = { 0x76, 0xf0, 0x76 };
    assert_queue(&keyboard, escape, sizeof(escape));
    host_stdin_tick(&keyboard.host_stdin, &keyboard, 0);
    assert(keyboard.host_stdin.terminal_state == HOST_STDIN_TERMINAL_NORMAL);

    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, 0x1b));
    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, '['));
    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, 'x'));
    assert(keyboard.host_stdin.terminal_state == HOST_STDIN_TERMINAL_REPLAY);
    host_stdin_tick(&keyboard.host_stdin, &keyboard, 0);
    host_stdin_tick(&keyboard.host_stdin, &keyboard, 0);
    host_stdin_tick(&keyboard.host_stdin, &keyboard, 0);
    host_stdin_tick(&keyboard.host_stdin, &keyboard, 0);
    const uint8_t replay[] = {
        0x76, 0xf0, 0x76,
        0x54, 0xf0, 0x54,
        0x22, 0xf0, 0x22,
    };
    assert_queue(&keyboard, replay, sizeof(replay));
    assert(keyboard.host_stdin.terminal_state == HOST_STDIN_TERMINAL_NORMAL);
    keyboard_deinit(&keyboard);
}

static void test_terminal_sequence_backpressure(void)
{
    keyboard_t keyboard = { 0 };
    pio_t pio = { 0 };
    assert(keyboard_init(&keyboard, &pio, false) == 0);
    for(size_t i = 0; i < FIFO_SIZE - 4; i++) {
        assert(fifo_push(&keyboard.queue, 0xaa));
    }

    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, 0x1b));
    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, '['));
    assert(!host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, 'A'));
    assert(fifo_size(&keyboard.queue) == FIFO_SIZE - 4);
    uint8_t discarded;
    assert(fifo_pop(&keyboard.queue, &discarded));
    assert(host_stdin_terminal_byte(&keyboard.host_stdin, &keyboard, 'A'));
    assert(keyboard.host_stdin.terminal_state == HOST_STDIN_TERMINAL_NORMAL);
    assert(fifo_size(&keyboard.queue) == FIFO_SIZE);
    keyboard_deinit(&keyboard);
}

int main(void)
{
    test_disabled_stdin_uses_no_host_calls();
    test_pipe_polling_and_backpressure();
    test_interactive_tty();
    test_initialization_failure_rollback();
    test_ascii_translation();
    test_crlf_and_unsupported_bytes();
    test_atomic_queue_backpressure();
    test_window_and_stdin_share_fifo();
    test_terminal_navigation_and_functions();
    test_terminal_modifiers();
    test_terminal_escape_timeout_and_replay();
    test_terminal_sequence_backpressure();
    return 0;
}
