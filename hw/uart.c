/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdint.h>

#include "hw/host.h"
#include "utils/helpers.h"
#include "hw/pio.h"
#include "hw/uart.h"

static void transfer_complete(uart_t* uart)
{
    char c = 0;
    for(uint8_t i = 1; i < 10; i++) {
        c |= uart->tx_fifo[i] << (i - 1);
    }

    if(c == 0) return; // can't print null, :shrug:

    if (uart->host && uart->host->uart_byte) uart->host->uart_byte(uart->host->userdata, (uint8_t) c);
}

void write_tx(void* arg, pio_t* pio, bool read, int pin, int bit, bool transition)
{
    uart_t* uart = arg;
    (void)transition;
    (void)pio;

    if(read) return;
    if(pin != IO_UART_TX_PIN) return;

    if(bit == 1 && uart->tx_pos == 0) return;

    uart->tx_fifo[uart->tx_pos++] = bit;
    if(uart->tx_pos == 10) {
        transfer_complete(uart);
        uart->tx_pos = 0;
    }
}

void read_rx(void* arg, pio_t* pio, bool read, int pin, int bit, bool transition)
{
    // TODO: do nothing ... should we even define this?
    (void)pio;
    uart_t* uart = arg;
    hw_log(uart->host, false, 0, "uart:read: %d, %d, %d, %d\n", read, pin, bit, transition);
}

int uart_init(uart_t* uart, pio_t* pio)
{
    uart->tx_pos = 0;
    pio_set_b_pin(pio, IO_UART_RX_PIN, 1);

    pio_listen_b_pin(pio, IO_UART_TX_PIN, write_tx, uart);
    pio_listen_b_pin(pio, IO_UART_RX_PIN, read_rx, uart);

    return 0;
}
