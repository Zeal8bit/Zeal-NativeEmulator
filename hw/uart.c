#include <stdint.h>

#include "utils/log.h"
#include "utils/helpers.h"
#include "hw/pio.h"
#include "hw/uart.h"

static unsigned long bit_tstates = 0;

static uint8_t tx_fifo[10];
static uint8_t tx_pos = 0;

static void transfer_complete(void) {
    char c = 0;
    for(uint8_t i = 1; i < 10; i++) {
        c |= tx_fifo[i] << (i - 1);
    }

    if(c == 0) return; // can't print null, :shrug:

    log_printf("%c", c);
    fflush(stdout);
}

void write_tx(void* arg, pio_t* pio, bool read, int pin, int bit, bool transition)
{
    (void)arg;
    (void)transition;
    (void)pio;

    if(read) return;
    if(pin != IO_UART_TX_PIN) return;

    if(bit == 1 && tx_pos == 0) return;

    tx_fifo[tx_pos++] = bit;
    if(tx_pos == 10) {
        transfer_complete();
        tx_pos = 0;
    }
}

void read_rx(void* arg, pio_t* pio, bool read, int pin, int bit, bool transition)
{
    // TODO: do nothing ... should we even define this?
    (void)pio;
    (void)arg;
    log_printf("uart:read: %d, %d, %d, %d\n", read, pin, bit, transition);
}

int uart_init(uart_t* uart, pio_t* pio)
{
    (void)uart;
    pio_set_b_pin(pio, IO_UART_RX_PIN, 1);
    bit_tstates = us_to_tstates(BAUDRATE_US) + 1;

    pio_listen_b_pin(pio, IO_UART_TX_PIN, write_tx, uart);
    pio_listen_b_pin(pio, IO_UART_RX_PIN, read_rx, uart);

    return 0;
}
