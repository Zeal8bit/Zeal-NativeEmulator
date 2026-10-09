/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include "hw/zvb/zvb_spi.h"
#include "utils/log.h"

#define SPI_RESET_CLK_DIV                (10)
#define SPI_LAST_BIT                     (7)
#define SPI_TRANSFER_INDEX_MASK          (15)
#define TF_COMMAND_PREFIX_MASK           (0xc0)
#define TF_COMMAND_PREFIX                (0x40)
#define TF_COMMAND_INDEX_MASK            (0x3f)
#define TF_CRC_BYTES                     (2)
#define TF_CRC16_HIGH_BIT                (0x8000)
#define TF_CRC16_POLYNOMIAL              (0x1021)
#define TF_WRITE_ACCEPTED                (0x05)
#define TF_WRITE_ERROR                   (0x0d)
#define TF_BUSY_BYTE                     (0x00)
#define TF_IDLE_BYTE                     (0xff)
#define TF_WRITE_REPLY_LEN               (3)

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
static void zvb_tf_receive_byte(zvb_spi_t* spi, uint8_t data);

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
    spi->sclk = false;
    spi->busy = false;
    spi->transfer_index = 0;
    spi->period_counter = 0;
    spi->tf.write_index = 0;
    spi->tf.command_index = 0;
    spi->tf.reply_len = 0;
    spi->tf.reply_idx = 0;
    /* Reset TF card */
    spi->tf.state = TF_IDLE;
}

int zvb_spi_load_tf_image(zvb_spi_t* spi, const char* filename)
{
    if (spi == NULL || filename == NULL) {
        return 1;
    }

    /* Open it in both read and write */
    FILE* image = fopen(filename, "r+b");
    if (image == NULL) {
        log_perror("[TF] Could not open TF Card image");
        return 1;
    }

    /* Save the size of the file */
    if (fseek(image, 0, SEEK_END) != 0) {
        fclose(image);
        return 1;
    }
    const long size = ftell(image);
    if (size < 0 || fseek(image, 0, SEEK_SET) != 0) {
        fclose(image);
        return 1;
    }
    if (spi->tf.img) {
        fclose(spi->tf.img);
    }
    spi->tf.img = image;
    spi->tf.img_size = (size_t)size;
    zvb_tf_deassert(spi);
    log_printf("[TF] %s loaded successfully\n", filename);
    return 0;
}


void zvb_spi_write(zvb_spi_t* spi, uint32_t addr, uint8_t value)
{
    uint_fast8_t index;
    /* We may want to interpret the value as a control */
    zvb_spi_ctrl_t ctrl = { .raw = value };

    switch(addr) {
        case SPI_REG_CTRL:
            /* The TF card is connected to CS0. */
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
                spi->clk_div = SPI_RESET_CLK_DIV;
                spi->ram_len = 0;
                spi->tf_cs = 0;
                spi->sclk = false;
                spi->busy = false;
                spi->transfer_index = 0;
                spi->period_counter = 0;
                zvb_tf_deassert(spi);
            } else if (ctrl.start && spi->ram_len) {
                spi->busy = true;
                spi->transfer_index = 0;
                spi->bit_index = SPI_LAST_BIT;
                spi->mosi = spi->ram_wr.data[0] >> SPI_LAST_BIT;
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
        case SPI_REG_OPERAND:
            spi->operand = value;
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
            return spi->busy;
        case SPI_REG_CLK_DIV:
            return spi->clk_div;
        case SPI_REG_RAM_LEN:
            return spi->ram_len;
        case SPI_REG_OPERAND:
            return spi->operand;
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
    zvb_tf_t* tf = &spi->tf;
    tf->write_index = 0;
    tf->command_index = 0;
    tf->reply_len = 0;
    tf->reply_idx = 0;
    if (tf->state != TF_CMD55_RECEIVED) {
        tf->state = TF_IDLE;
    }
}


static uint8_t zvb_tf_next_byte(zvb_tf_t* tf)
{
    if (tf->reply_idx < tf->reply_len) {
        const uint8_t data = tf->reply[tf->reply_idx++];
        if (tf->reply_idx == tf->reply_len &&
            (tf->state == TF_READ_BLOCK || tf->state == TF_WRITE_BLOCK_SEND_RESP)) {
            tf->state = TF_IDLE;
        }
        return data;
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

static bool zvb_tf_seek_block(zvb_tf_t* tf, uint32_t sector)
{
    /* Validate a complete sector before multiplying; never wrap a large
     * guest address or extend the host image with an out-of-range write. */
    if (!tf->img || (uint64_t)sector >= tf->img_size / TF_BLK_SIZE) {
        return false;
    }
    const uint64_t offset = (uint64_t)sector * TF_BLK_SIZE;
    return offset <= LONG_MAX && fseek(tf->img, (long)offset, SEEK_SET) == 0;
}

static uint16_t zvb_tf_data_crc(const uint8_t* data)
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < TF_BLK_SIZE; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint32_t bit = 0; bit < 8; bit++) {
            crc = (crc << 1) ^ (crc & TF_CRC16_HIGH_BIT ? TF_CRC16_POLYNOMIAL : 0);
        }
    }
    return crc;
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
                if (!zvb_tf_seek_block(tf, param)) {
                    r1.addr_err = 1;
                    zvb_r1_response(tf, r1.raw);
                    return;
                }
                tf->reply[0] = 0xFF;     // Dummy byte
                tf->reply[1] = 0x00;     // ACK!
                tf->reply[2] = TF_DATA_TOKEN;     // Set as ready!
                const size_t rd = fread(tf->reply + TF_BLK_DUMMY_BYTES, 1, TF_BLK_SIZE, tf->img);
                if (rd < TF_BLK_SIZE) {
                    r1.param_err = 1;
                    zvb_r1_response(tf, r1.raw);
                    return;
                }
                tf->state = TF_READ_BLOCK;
                const uint16_t crc = zvb_tf_data_crc(tf->reply + TF_BLK_DUMMY_BYTES);
                tf->reply[TF_BLK_DUMMY_BYTES + TF_BLK_SIZE] = crc >> 8;
                tf->reply[TF_BLK_DUMMY_BYTES + TF_BLK_SIZE + 1] = crc;
                tf->reply_idx = 0;
                tf->reply_len = TF_BLK_SIZE + TF_BLK_DUMMY_BYTES + TF_CRC_BYTES;
            }
            break;
        case TF_WRITE_BLK:
            /* Read block */
            if (tf->state != TF_IDLE) {
                r1.ill_cmd = 1;
                zvb_r1_response(tf, r1.raw);
            } else {
                if (!zvb_tf_seek_block(tf, param)) {
                    r1.addr_err = 1;
                    zvb_r1_response(tf, r1.raw);
                    break;
                }
                tf->state = TF_WRITE_BLOCK_WAIT_TOK;
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


/* The card is a byte-stream endpoint; commands may span hardware transfers. */
static void zvb_tf_receive_byte(zvb_spi_t* spi, uint8_t data)
{
    zvb_tf_t* tf = &spi->tf;
    if (!spi->tf_cs || !tf->img) {
        return;
    }
    if (tf->state == TF_WRITE_BLOCK_WAIT_TOK) {
        if (data == TF_DATA_TOKEN) {
            tf->state = TF_WRITE_BLOCK;
            tf->write_index = 0;
        }
        return;
    }
    if (tf->state == TF_WRITE_BLOCK) {
        tf->reply[tf->write_index++] = data;
        if (tf->write_index == TF_BLK_SIZE + TF_CRC_BYTES) {
            const size_t written = fwrite(tf->reply, 1, TF_BLK_SIZE, tf->img);
            tf->reply[0] = written == TF_BLK_SIZE ? TF_WRITE_ACCEPTED : TF_WRITE_ERROR;
            tf->reply[1] = TF_BUSY_BYTE;
            tf->reply[2] = TF_IDLE_BYTE;
            tf->reply_idx = 0;
            tf->reply_len = TF_WRITE_REPLY_LEN;
            tf->state = TF_WRITE_BLOCK_SEND_RESP;
        }
        return;
    }
    if (tf->state == TF_READ_BLOCK || tf->state == TF_WRITE_BLOCK_SEND_RESP) {
        return;
    }
    if (!tf->command_index && (data & TF_COMMAND_PREFIX_MASK) != TF_COMMAND_PREFIX) {
        return;
    }
    tf->command[tf->command_index++] = data;
    if (tf->command_index == sizeof(tf->command)) {
        const uint32_t param = ((uint32_t)tf->command[1] << 24) |
                               ((uint32_t)tf->command[2] << 16) |
                               ((uint32_t)tf->command[3] << 8) | tf->command[4];
        tf->command_index = 0;
        zvb_tf_process_command(spi, tf->command[0] & TF_COMMAND_INDEX_MASK, param);
    }
}

void zvb_spi_clock(zvb_spi_t* spi)
{
    if (!spi->busy) {
        return;
    }
    if (spi->period_counter != spi->clk_div - 1) {
        spi->period_counter++;
        return;
    }
    spi->period_counter = 0;
    if (!spi->sclk) {
        /* Capture MISO on rising edges, including wrapping eight-byte RAM. */
        if (spi->bit_index == SPI_LAST_BIT) {
            spi->incoming = spi->tf_cs && spi->tf.img ? zvb_tf_next_byte(&spi->tf) : TF_IDLE_BYTE;
            spi->outgoing = 0;
        }
        const uint32_t mask = 1u << spi->bit_index;
        uint8_t* input = &spi->ram_rd.data[spi->transfer_index & (SPI_RAM_LEN - 1)];
        *input = (*input & ~mask) | (spi->incoming & mask);
        spi->outgoing |= spi->mosi << spi->bit_index;
        if (!spi->bit_index) {
            zvb_tf_receive_byte(spi, spi->outgoing);
            spi->transfer_index = (spi->transfer_index + 1) & SPI_TRANSFER_INDEX_MASK;
        }
        spi->bit_index = (spi->bit_index - 1) & SPI_LAST_BIT;
    } else {
        /* Advance MOSI on falling edges, or finish after the last byte. */
        if (spi->transfer_index == spi->ram_len) {
            spi->busy = false;
        } else {
            spi->mosi = (spi->ram_wr.data[spi->transfer_index & (SPI_RAM_LEN - 1)] >> spi->bit_index) & 1;
        }
    }
    spi->sclk = !spi->sclk;
}
