/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "hw/host.h"
extern const hw_host_t frontend_host;

/* Optional observer of every byte the machine sends on the UART, called
 * after it was printed to stdout. NULL (default) when unused. */
extern void (*frontend_uart_tap)(void* userdata, uint8_t byte);
extern void* frontend_uart_tap_userdata;
