/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include <unistd.h>
#include "frontend/common/host.h"
#include "utils/config.h"

static void host_log(void* userdata, bool error, int verbosity, const char* format, va_list args)
{
    (void) userdata;
    if (verbosity > config.arguments.verbose) return;
    vfprintf(error ? stderr : stdout, format, args);
}

static int host_read_byte(void* userdata)
{
    (void) userdata;
    uint8_t byte;
    return read(STDIN_FILENO, &byte, 1) == 1 ? byte : -1;
}

void (*frontend_uart_tap)(void* userdata, uint8_t byte) = NULL;
void* frontend_uart_tap_userdata = NULL;

static void host_uart_byte(void* userdata, uint8_t byte)
{
    (void) userdata;
    printf("%c", byte);
    fflush(stdout);
    if (frontend_uart_tap) frontend_uart_tap(frontend_uart_tap_userdata, byte);
}

const hw_host_t frontend_host = {
    .log = host_log,
    .read_byte = host_read_byte,
    .uart_byte = host_uart_byte,
};
