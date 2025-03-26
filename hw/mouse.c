#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "hw/mouse.h"

#define ABS(x) ((x) < 0) ? (-(x)) : (x)

// #define debug printf
#define debug(...)


/**
 * @brief Specify a booting time for the mouse
 */
#define MOUSE_BOOT_PERIOD     US_TO_TSTATES(500000)
/**
 * @brief Make the assumption that the mouse send data every 12ms
 */
#define MOUSE_STATE_PERIOD    US_TO_TSTATES(16000)
/**
 * @brief Send one byte per millisecond
 */
#define MOUSE_BYTE_PERIOD     US_TO_TSTATES(1000)


/**
 * @brief Index of the PS/2 mouse register to send to the VM
 */
#define BUTTON_BYTE 0
#define X_MOVE_BYTE 1
#define Y_MOVE_BYTE 2
#define Z_MOVE_BYTE 3

/**
 * @brief Type of the byte0
 */
typedef union {
    struct {
        int lmb : 1;
        int rmb : 1;
        int mmb : 1;
        int one : 1;
        int x_sign : 1;
        int y_sign : 1;
        int x_ovf : 1;
        int y_ovf : 1;
    };
    uint8_t raw;
} mouse_byte0_t;


/**
 * @brief Check whether the mouse is still booting
 */
static inline bool mouse_booting(mouse_t* mouse, int delta)
{
    if (mouse->booting_elapsed >= MOUSE_BOOT_PERIOD) {
        return false;
    }
    mouse->booting_elapsed += delta;
    return mouse->booting_elapsed < MOUSE_BOOT_PERIOD;
}


static uint8_t io_read(device_t* dev, uint32_t addr)
{
    mouse_t* mouse = (mouse_t*) dev;

    pio_set_b_pin(mouse->pio, IO_PS2_PIN, 1);

    (void) addr;
    return mouse->shift_register;
}


int mouse_init(mouse_t* mouse, pio_t* pio, bool ps2_interface)
{
    memset(mouse, 0, sizeof(mouse_t));
    device_init_io(DEVICE(mouse), "mouse_dev", io_read, NULL, 0x10);
    pio_set_b_pin(pio, IO_PS2_PIN, 1);
    mouse->pio = pio;

    (void) ps2_interface;
    return 0;
}


void mouse_tick(mouse_t* mouse, int delta)
{
    /* If we aren't currently sending anything ... do nothing */
    if (!mouse->state_ready) {
        return;
    }

    mouse->sent_elapsed += delta;
    if (mouse->sent_elapsed < MOUSE_BYTE_PERIOD) {
        return;
    }

    /* Ready to send the next byte */
    uint8_t sending = 0;
    switch (mouse->sent)
    {
        case BUTTON_BYTE: {
            mouse_byte0_t byte0 = {
                .lmb = mouse->last.lmb,
                .rmb = mouse->last.rmb,
                .mmb = mouse->last.mmb,
                .one = 1,
                .x_sign = mouse->diff_x < 0,
                .y_sign = mouse->diff_y < 0,
                .x_ovf = ABS(mouse->diff_x) > 255,
                .y_ovf = ABS(mouse->diff_y) > 255,
            };
            sending = byte0.raw;
            break;
        }

        case X_MOVE_BYTE:
            sending = mouse->diff_x & 0xff;
            break;

        case Y_MOVE_BYTE:
            sending = mouse->diff_y & 0xff;
            break;

        case Z_MOVE_BYTE:
            sending = mouse->last.z & 0xff;
            break;

        default:
            printf("[Mouse] Warning: VM is reading more bytes than available");
            return;
    }
    printf("[%d] = %02x\n", mouse->sent, sending);

    pio_set_b_pin(mouse->pio, IO_PS2_PIN, 0);
    mouse->shift_register = sending;
    mouse->sent++;
    mouse->sent_elapsed = 0;

    /* If we finished processing the last byte, disable byte processing */
    if (mouse->sent == 4) {
        printf("\n");
        mouse->state_ready = false;
    }
}


void mouse_send_next(mouse_t* mouse, const mouse_state_t* state, int delta)
{
    if (mouse == NULL || mouse_booting(mouse, delta)) {
        return;
    }

    mouse->state_elapsed += delta;
    if (mouse->state_elapsed < MOUSE_STATE_PERIOD) {
        /* Haven't reached the period yet, don't check anything */
        return;
    }

    /* Check if any change occured */
    if (memcmp(&mouse->last, state, sizeof(mouse_state_t)) == 0) {
        return;
    }

    /* Save the difference */
    mouse->diff_x = state->x - mouse->last.x;
    mouse->diff_y = state->y - mouse->last.y;

    /* Copy the new state to the mouse structure */
    memcpy(&mouse->last, state, sizeof(mouse_state_t));

    if (mouse->sent > 0 && mouse->sent < 4) {
        printf("[Mouse] Warning: VM has not read mouse data fast enough, losing data\n");
    }

    /* Inform the VM of this change */
    mouse->sent = 0;
    mouse->sent_elapsed = 0;
    mouse->state_elapsed = 0;
    mouse->state_ready = true;
}
