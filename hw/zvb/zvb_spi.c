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
#define SPI_CLOCK_NS                     (20U)
#define SPI_BYTE_CLOCKS                  (16U)
#define SPI_RAM_LEN_MASK                 (15)
#define SPI_RAM_RESET_INDEXES            (0x80)
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
static void zvb_spi_complete_byte(void* userdata);

static void zvb_spi_schedule_byte(zvb_spi_t* spi, uint64_t start_ns)
{
    spi->outgoing = spi->ram_wr.data[spi->transfer_index % SPI_RAM_LEN];
    vtimer_schedule_at_ns(&spi->event, start_ns + SPI_BYTE_CLOCKS * SPI_CLOCK_NS * spi->clk_div);
}

void zvb_spi_init(zvb_spi_t* spi)
{
    assert(spi != NULL);
    memset(spi, 0, sizeof(*spi));
    vtimer_init_node(&spi->event, zvb_spi_complete_byte, spi);
    zvb_spi_reset(spi);
}

void zvb_spi_reset(zvb_spi_t* spi)
{
    vtimer_cancel(&spi->event);
    spi->clk_div = SPI_RESET_CLK_DIV;
    spi->ram_rd.idx = 0;
    spi->ram_wr.idx = 0;
    spi->ram_len = 0;
    spi->tf_cs = 0;
    spi->busy = false;
    spi->transfer_index = 0;
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
                vtimer_cancel(&spi->event);
                spi->clk_div = SPI_RESET_CLK_DIV;
                spi->ram_len = 0;
                spi->tf_cs = 0;
                spi->busy = false;
                spi->transfer_index = 0;
                zvb_tf_deassert(spi);
            } else if (ctrl.start && spi->ram_len && !spi->busy) {
                spi->busy = true;
                spi->transfer_index = 0;
                /* Latch the request length; later writes configure the next transfer. */
                spi->transfer_len = spi->ram_len;
#if 0
                log_printf("[ZVB][SPI] Data: %x, %x, %x, %x, %x, %x, %x, %x, len: %d\n",
                           spi->ram_wr.data[0],
                           spi->ram_wr.data[1],
                           spi->ram_wr.data[2],
                           spi->ram_wr.data[3],
                           spi->ram_wr.data[4],
                           spi->ram_wr.data[5],
                           spi->ram_wr.data[6],
                           spi->ram_wr.data[7],
                           spi->transfer_len);
#endif
                zvb_spi_schedule_byte(spi, vtimer_now_ns());
            }
            break;
        case SPI_REG_CLK_DIV:
            /* Make sure the divider is never 0 */
            spi->clk_div = value ? value : 1;
            break;
        case SPI_REG_RAM_LEN:
            spi->ram_len = value & SPI_RAM_LEN_MASK;
            /* If the highest bit is 1, clear the indexes */
            if (value & SPI_RAM_RESET_INDEXES) {
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
    if (tf->state == TF_READ_BLOCK && tf->reply_idx < TF_BLK_DUMMY_BYTES + TF_BLK_SIZE) {
        const int received = tf->reply_idx > TF_BLK_DUMMY_BYTES ? tf->reply_idx - TF_BLK_DUMMY_BYTES : 0;
        log_err_printf("[TF] Warning: read block command did not read the whole block! (%d/%d)\n",
                       received, TF_BLK_SIZE);
    }
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
        log_err_printf("[TF] Invalid sector: 0x%x (image size: 0x%zx)\n", sector, tf->img_size);
        return false;
    }
    const uint64_t offset = (uint64_t)sector * TF_BLK_SIZE;
    if (offset > LONG_MAX) {
        log_err_printf("[TF] Sector offset exceeds host file range: 0x%x\n", sector);
        return false;
    }
    if (fseek(tf->img, (long)offset, SEEK_SET) != 0) {
        log_perror("[TF] Could not seek into image");
        return false;
    }
    return true;
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
                    log_err_printf("[TF] Warning could only read %zu/%d bytes from the image file\n", rd, TF_BLK_SIZE);
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
#if DEBUG_WRITE
                log_printf("[TF] Write block, offset: 0x%llx (sector: %x)\n",
                           (unsigned long long)param * TF_BLK_SIZE, param);
#endif
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
#if 0
            log_printf("[TF] Writing data: \n");
            for (int i = 0; i < TF_BLK_SIZE; i++) {
                log_printf("%x, ", tf->reply[i]);
            }
            log_printf("\n");
#endif
            const size_t written = fwrite(tf->reply, 1, TF_BLK_SIZE, tf->img);
            if (written < TF_BLK_SIZE) {
                log_err_printf("[TF] Warning could only write %zu/%d bytes to the image file\n", written, TF_BLK_SIZE);
            }
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

/* Complete whole bytes; no clock edges or partially received bits are modeled. */
static void zvb_spi_complete_byte(void* userdata)
{
    zvb_spi_t* spi = userdata;
    const uint8_t index = spi->transfer_index % SPI_RAM_LEN;
    spi->ram_rd.data[index] = spi->tf_cs && spi->tf.img ? zvb_tf_next_byte(&spi->tf) : TF_IDLE_BYTE;
    zvb_tf_receive_byte(spi, spi->outgoing);
    spi->transfer_index++;
    if (spi->transfer_index == spi->transfer_len) {
        spi->busy = false;
    } else {
        zvb_spi_schedule_byte(spi, spi->event.deadline);
    }
}
