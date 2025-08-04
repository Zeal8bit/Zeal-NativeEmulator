/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "hw/zvb/zvb_spi.h"
#include "utils/log.h"

#define DEBUG_CMD       0
#define DEBUG_WRITE     0

#define TF_CMD_MASK     0x40
#define TF_CMD0_CRC     0x95
#define TF_CMD1_CRC     0xF9
#define TF_CMD55_CRC    0x65
#define TF_CMD58_CRC    0x95
#define TF_ILL_CMD      0x05

#define TF_BLK_SIZE     512


#define TF_READ_BLK         17
#define TF_WRITE_BLK        24
#define TF_WRITE_MUL_BLK    25


/**
 * @brief Number of additional bytes read by the host when a block is read
 */
#define TF_BLK_DUMMY_BYTES  3

static void zvb_tf_deassert(zvb_spi_t* spi);
static void zvb_tf_start(zvb_spi_t* spi);

void zvb_spi_init(zvb_spi_t* spi)
{
    assert(spi != NULL);
    memset(spi, 0, sizeof(*spi));
    zvb_spi_reset(spi);
}

void zvb_spi_reset(zvb_spi_t* spi)
{
    spi->clk_div = 10;
    spi->ram_rd.idx = 0;
    spi->ram_wr.idx = 0;
    spi->ram_len = 0;
    spi->tf_cs = 0;
    /* Reset TF card */
    spi->tf.state = TF_IDLE;
}

int zvb_spi_load_tf_image(zvb_spi_t* spi, const char* filename)
{
    if (spi == NULL || filename == NULL) {
        return 1;
    }

    /* Open it in both read and write */
    spi->tf.img = fopen(filename, "r+");
    if (spi->tf.img == NULL) {
        log_perror("[TF] Could not open TF Card image");
        return 1;
    }

    log_printf("[TF] %s loaded successfully\n", filename);

    /* Save the size of the file */
    fseek(spi->tf.img, 0, SEEK_END);
    spi->tf.img_size = ftell(spi->tf.img);
    fseek(spi->tf.img, 0, SEEK_SET);
    return 0;
}


void zvb_spi_write(zvb_spi_t* spi, uint32_t addr, uint8_t value)
{
    uint_fast8_t index;
    /* We may want to interpret the value as a control */
    zvb_spi_ctrl_t ctrl = { .raw = value };

    switch(addr) {
        case SPI_REG_CTRL:
            /* Only accept TF CS (0) */
            if (ctrl.csel == 0) {
                /* CS_START has the priority over CS_END */
                if (ctrl.cstart) {
                    spi->tf_cs = 1;
                } else if (ctrl.cend) {
                    spi->tf_cs = 0;
                    zvb_tf_deassert(spi);
                }
            }

            if (ctrl.reset) {
                spi->clk_div = 2;
                spi->ram_len = 0;
                spi->ram_rd.idx = 0;
                spi->ram_wr.idx = 0;
            } else if (ctrl.start && spi->tf_cs == 1) {
                zvb_tf_start(spi);
            }
            break;
        case SPI_REG_CLK_DIV:
            /* Make sure the divider is never 0 */
            spi->clk_div = value ? value : 1;
            break;
        case SPI_REG_RAM_LEN:
            spi->ram_len = value & 0xf;
            /* If the highest bit is 1, clear the indexes */
            if (value & 0x80) {
                spi->ram_rd.idx = 0;
                spi->ram_wr.idx = 0;
            }
            break;
        case SPI_REG_CHECKSUM:
            break;
        case SPI_REG_RAM_FIFO:
            index = spi->ram_wr.idx;
            assert(index < SPI_RAM_LEN);
            spi->ram_wr.data[index] = value;
            spi->ram_wr.idx = (index + 1) % SPI_RAM_LEN;
            break;
        case SPI_REG_RAM_FROM ... SPI_REG_RAM_TO:
            assert(addr - SPI_REG_RAM_FROM < SPI_RAM_LEN);
            spi->ram_wr.data[addr - SPI_REG_RAM_FROM] = value;
            break;
        default:
            break;
    }
}


uint8_t zvb_spi_read(zvb_spi_t* spi, uint32_t addr)
{
    switch(addr) {
        case SPI_REG_VERSION:
            return SPI_VERSION;
        case SPI_REG_CTRL:
            /* Always return the state as IDLE (0) */
            return 0;
        case SPI_REG_CLK_DIV:
            return spi->clk_div;
        case SPI_REG_RAM_LEN:
            return spi->ram_len;
        case SPI_REG_CHECKSUM:
            return 0; // TODO?
        case SPI_REG_RAM_FIFO:
            assert(spi->ram_rd.idx < SPI_RAM_LEN);
            const uint8_t data = spi->ram_rd.data[spi->ram_rd.idx];
            spi->ram_rd.idx = (spi->ram_rd.idx + 1) % SPI_RAM_LEN;
            return data;
        case SPI_REG_RAM_FROM ... SPI_REG_RAM_TO:
            assert(addr - SPI_REG_RAM_FROM < SPI_RAM_LEN);
            return spi->ram_rd.data[addr - SPI_REG_RAM_FROM];
    }
    return 0;
}


/**
 * TF Emulation Related
 */
typedef union {
    struct {
        uint32_t idle       : 1;
        uint32_t erase      : 1;
        uint32_t ill_cmd    : 1;
        uint32_t crc_err    : 1;
        uint32_t erase_err  : 1;
        uint32_t addr_err   : 1;
        uint32_t param_err  : 1;
    };
    uint8_t raw;
} r1_resp;


static void zvb_tf_deassert(zvb_spi_t* spi)
{
    /* Make sure the TF Card has been initialized at least once */
    if (spi->tf.state == TF_READ_BLOCK || spi->tf.state == TF_WRITE_BLOCK_SEND_RESP) {
        /* Make sure the block read (or write) processed a whole block. `reply_idx` points to the next byte to read! */
        if (spi->tf.state == TF_READ_BLOCK && spi->tf.reply_idx - TF_BLK_DUMMY_BYTES != TF_BLK_SIZE){
            log_err_printf("[TF] Warning: read block command did not read the whole block! (%d/%d)\n",
                    spi->tf.reply_idx - TF_BLK_DUMMY_BYTES, TF_BLK_SIZE);
        }
        spi->tf.state = TF_IDLE;
    }
}


static uint8_t zvb_tf_next_byte(zvb_tf_t* tf)
{
    if (tf->reply_idx < tf->reply_len) {
        return tf->reply[tf->reply_idx++];
    }

    /* Dummy byte */
    return 0xFF;
}


static void zvb_r1_response(zvb_tf_t* tf, uint8_t r1)
{
    tf->reply[0] = 0xFF;
    tf->reply[1] = r1;
    tf->reply_idx = 0;
    tf->reply_len = 2;
}


static void zvb_tf_process_command(zvb_spi_t* spi, uint32_t command, uint32_t param)
{
    zvb_tf_t* tf = &spi->tf;
    r1_resp r1 = { .idle = 1 };
    int idx = 0;
#if DEBUG_CMD
    static struct {
        uint32_t cmd;
        uint32_t param;
    } former_command = { 0 };
    static int former_count = 1;
    if (former_command.cmd == command && former_command.param == param) {
        former_count++;
    } else {
        former_count = 1;
    }
    log_printf("[TF] Command: %d (0x%x), param: %x, count: %d\n",
        command, command | TF_CMD_MASK, param, former_count);
    former_command.cmd = command;
    former_command.param = param;
#endif

    switch (command) {
        case 0:
            /* Always accept reset command  */
            tf->state = TF_IDLE;
            zvb_r1_response(tf, r1.raw);
            break;
        case 8:
            /* Check voltage range. TODO: if in IDLE only! */
            tf->reply[idx++] = 0xFF;
            tf->reply[idx++] = r1.raw;
            /* 32-bit argument */
            tf->reply[idx++] = 0x00;
            tf->reply[idx++] = 0x00;
            tf->reply[idx++] = 0x01;
            tf->reply[idx++] = 0xAA;
            tf->reply_idx = 0;
            tf->reply_len = idx;
            break;
        case 16:
            /* Change block size, only accept 512 for now */
            if (tf->state != TF_IDLE) {
                r1.ill_cmd = 1;
                zvb_r1_response(tf, r1.raw);
            } else if (param != 512) {
                log_err_printf("[TF] Cannot set block size to another value than 512 bytes\n");
                r1.param_err = 1;
                zvb_r1_response(tf, r1.raw);
            } else {
                r1.raw = 0;
                zvb_r1_response(tf, r1.raw);
            }
            break;
        case TF_READ_BLK:
            /* Read block */
            if (tf->state != TF_IDLE) {
                r1.ill_cmd = 1;
                zvb_r1_response(tf, r1.raw);
            } else {
                tf->state = TF_READ_BLOCK;
                /* TODO: check the size of the file against the block number */
                const uint32_t offset = param * TF_BLK_SIZE;
                if (fseek(tf->img, offset, SEEK_SET) < 0) {
                    log_perror("[TF] Could not seek into image for reading");
                    r1.param_err = 1;
                    zvb_r1_response(tf, r1.raw);
                    return;
                }
                tf->reply[0] = 0xFF;     // Dummy byte
                tf->reply[1] = 0x00;     // ACK!
                tf->reply[2] = TF_DATA_TOKEN;     // Set as ready!
                int rd = fread(tf->reply + TF_BLK_DUMMY_BYTES, 1, TF_BLK_SIZE, tf->img);
                if (rd < TF_BLK_SIZE) {
                    log_err_printf("[TF] Warning could only read %d/%d bytes from the image file\n", rd, TF_BLK_SIZE);
                }
                tf->reply_idx = 0;
                tf->reply_len = TF_BLK_SIZE + TF_BLK_DUMMY_BYTES;
            }
            break;
        case TF_WRITE_BLK:
            /* Read block */
            if (tf->state != TF_IDLE) {
                r1.ill_cmd = 1;
                zvb_r1_response(tf, r1.raw);
            } else {
                tf->state = TF_WRITE_BLOCK_WAIT_TOK;
                /* TODO: check the size of the file against the block number */
                const size_t offset = param * TF_BLK_SIZE;
                if (offset >= tf->img_size) {
                    log_err_printf("[TF] Invalid write offset: 0x%lx/0x%lx\n", offset, tf->img_size);
                    r1.param_err = 1;
                    zvb_r1_response(tf, r1.raw);
                    tf->state = TF_IDLE;
                    break;
                }
#if DEBUG_WRITE
                log_printf("[TF] Write block, offset: 0x%lx (sector: %x)\n", offset, param);
#endif
                if (fseek(tf->img, offset, SEEK_SET) < 0) {
                    log_perror("[TF] Could not seek into image for writing");
                    r1.param_err = 1;
                    zvb_r1_response(tf, r1.raw);
                    return;
                }
                tf->reply[0] = 0xFF;     // Dummy byte
                tf->reply[1] = 0x00;     // ACK!
                tf->reply_idx = 0;
                tf->reply_len = 2;
            }
            break;
        case 55:
            /* Accept CMD55 in IDLE only */
            if (tf->state != TF_IDLE) {
                r1.ill_cmd = 1;
            } else {
                tf->state = TF_CMD55_RECEIVED;
            }
            zvb_r1_response(tf, r1.raw);
            break;
        case 41:
            /* Check if it is an ACMD41 */
            if (tf->state == TF_CMD55_RECEIVED) {
                /* Accept it directly */
                r1.raw = 0;
                zvb_r1_response(tf, r1.raw);
            } else {
                /* Else, failure */
                r1.ill_cmd = 1;
                zvb_r1_response(tf, r1.raw);
            }
            tf->state = TF_IDLE;
            break;

        case 59:
            /* Disable CRC command */
            if (tf->state != TF_IDLE) {
                r1.ill_cmd = 1;
            } else {
                r1.raw = 0;
            }
            zvb_r1_response(tf, r1.raw);
            break;

        case 58: // TODO?
        default:
            r1.ill_cmd = 1;
            zvb_r1_response(tf, r1.raw);
            if (tf->state != TF_WAIT_IDLE) {
                tf->state = TF_IDLE;
            }
            break;
    }
}


static void zvb_tf_start_write(zvb_spi_t* spi)
{
    const int length = spi->ram_len;
    int i = 0;

    if (spi->tf.state == TF_WRITE_BLOCK_WAIT_TOK) {
        /* Look for the data token */
        for (i = 0; i < length; i++) {
            if (spi->ram_wr.data[i] == TF_DATA_TOKEN) {
                spi->tf.reply_idx = 0;
                spi->tf.state = TF_WRITE_BLOCK;
                /* Make i point to the first data byte */
                i++;
                break;
            }
        }
    }

    if (spi->tf.state == TF_WRITE_BLOCK) {
        /* In total, we need to receive 512 bytes of data and 2 CRC bytes */
        for (; i < length && spi->tf.reply_idx < TF_BLK_SIZE + 2; i++) {
            spi->tf.reply[spi->tf.reply_idx++] = spi->ram_wr.data[i];
        }

        if (spi->tf.reply_idx == TF_BLK_SIZE + 2) {
            /* Finished receiving the block data, write it to the file, the file has already been seeked */
#if 0
            log_printf("[TF] Writing data: \n");
            for (int i = 0; i < TF_BLK_SIZE; i++) {
                log_printf("%x, ", spi->tf.reply[i]);
            }
            log_printf("\n");
#endif
            int wr = fwrite(spi->tf.reply, 1, TF_BLK_SIZE, spi->tf.img);
            if (wr < TF_BLK_SIZE) {
                log_err_printf("[TF] Warning could only write %d/%d bytes from the image file\n", wr, TF_BLK_SIZE);
            }
            spi->tf.state = TF_WRITE_BLOCK_SEND_RESP;
            spi->tf.reply_idx = 0;
        }
    }

    if (spi->tf.state == TF_WRITE_BLOCK_SEND_RESP) {
        /* The index i points to the response index we need to write */
        if (spi->tf.reply_idx == 0 && i < length) {
            /* Data response */
            spi->ram_rd.data[i++] = 0x5;
            spi->tf.reply_idx++;
        }
        if (spi->tf.reply_idx == 1 && i < length) {
            /* Busy flag */
            spi->ram_rd.data[i++] = 0x00;
            spi->tf.reply_idx++;
        }
        if (spi->tf.reply_idx >= 2 && i < length) {
            /* End */
            memset(spi->ram_rd.data + i, 0xFF, length - i);
        }
    }
}


static void zvb_tf_start(zvb_spi_t* spi)
{
    int i;
    const int length = spi->ram_len;
    /* If we don't have any TF card mounted, abort, fill the response with 0xFF */
    if (spi->tf.img == NULL) {
        memset(spi->ram_rd.data, 0xFF, length);
        return;
    }

    /* Look for the command in the array */\
    if (length > SPI_RAM_LEN) {
        log_err_printf("[TF] ERROR: length is bigger than HW array\n");
        return;
    }

#if 0
    log_printf("=================\n");
    log_printf("Data: %x, %x, %x, %x, %x, %x, %x, %x, len: %d\n",
            spi->ram_wr.data[0],
            spi->ram_wr.data[1],
            spi->ram_wr.data[2],
            spi->ram_wr.data[3],
            spi->ram_wr.data[4],
            spi->ram_wr.data[5],
            spi->ram_wr.data[6],
            spi->ram_wr.data[7],
            length);
#endif

    /* Special case for the WRITE state */
    if (zvb_tf_is_write(spi->tf.state)) {
        zvb_tf_start_write(spi);
        return;
    }

    for (i = 0; i < length; i++) {
        /* Fill the OUT array at the same time */
        spi->ram_rd.data[i] = zvb_tf_next_byte(&spi->tf);
        /* Highest bits must be 0b01xx */
        if ((spi->ram_wr.data[i] >> 6) == 0b01) {
            break;
        }
    }

    /* Reached the end of the array, didn't find a command, ignore */
    if (i == length) {
        return;
    }

    /* Parameters must be bundled with the command */
    if (i + 5 >= spi->ram_len) {
        log_err_printf("[TF] Parameters must be provided with the command\n");
        return;
    }

    /* Else, we found a command, extract it */
    uint32_t command = spi->ram_wr.data[i] & (TF_CMD_MASK - 1);
    uint32_t param = spi->ram_wr.data[i + 1] << 24 |
                     spi->ram_wr.data[i + 2] << 16 |
                     spi->ram_wr.data[i + 3] << 8 |
                     spi->ram_wr.data[i + 4] << 0;
    /* Ignore CRC for now */
    uint8_t crc = spi->ram_wr.data[i + 5];
    (void) crc;
    i += 6;
    /* Process the commands and continue filling the FIFO */
    zvb_tf_process_command(spi, command, param);
    for (; i < length; i++) {
        /* Fill the OUT array at the same time */
        spi->ram_rd.data[i] = zvb_tf_next_byte(&spi->tf);
    }
}
