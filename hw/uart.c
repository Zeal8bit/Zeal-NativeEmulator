#include <stdint.h>

#include "utils/helpers.h"
#include "hw/pio.h"
#include "hw/uart.h"

unsigned long bit_tstates = 0;

uint8_t tx_fifo[10];
uint8_t tx_pos = 0;

static void transfer_complete(void) {
    char c = 0;
    for(uint8_t i = 1; i < 10; i++) {
        c |= tx_fifo[i] << (i - 1);
    }

    if(c == 0) return; // can't print null, :shrug:

    printf("%c", c);
    fflush(stdout);
}

void write_tx(uint8_t read, uint8_t pin, uint8_t bit, uint8_t transition)
{
    (void)transition;

    if(read) return;
    if(pin != IO_UART_TX_PIN) return;

    if(bit == 1 && tx_pos == 0) return;

    tx_fifo[tx_pos++] = bit;
    if(tx_pos == 10) {
        transfer_complete();
        tx_pos = 0;
    }
}

void read_rx(uint8_t read, uint8_t pin, uint8_t bit, uint8_t transition)
{
    // TODO: do nothing ... should we even define this?
    (void)read;
    (void)pin;
    (void)bit;
    (void)transition;
    printf("uart:read: %d, %d, %d, %d\n", read, pin, bit, transition);
}

int uart_init(uart_t* uart, pio_t* pio)
{
    (void)uart;
    pio_set_b_pin(pio, IO_UART_RX_PIN, 1);
    bit_tstates = us_to_tstates(BAUDRATE_US) + 1;

    pio_listen_b_pin(pio, IO_UART_TX_PIN, write_tx);
    pio_listen_b_pin(pio, IO_UART_RX_PIN, read_rx);

    return 0;
}
