#pragma once

#include <stdint.h>

#define SPI_RAM_LEN     8
#define SPI_VERSION     1

#define TF_DATA_TOKEN   0xFE

/**
 * @brief I/O registers address, relative to the controller
 */
#define SPI_REG_VERSION     0
#define SPI_REG_CTRL        1
    #define SPI_REG_CTRL_START      (1 << 7)    // Start SPI transaction
    #define SPI_REG_CTRL_RESET      (1 << 6)    // Reset the SPI controller
    #define SPI_REG_CTRL_CS_START   (1 << 5)    // Assert chip select (low)
    #define SPI_REG_CTRL_CS_END     (1 << 4)    // De-assert chip select signal (high)
    #define SPI_REG_CTRL_CS_SEL     (1 << 3)    // Select among two chip selects (0 for TF card, 1 is reserved)
    #define SPI_REG_CTRL_RSV2       (1 << 2)
    #define SPI_REG_CTRL_RSV1       (1 << 1)
    #define SPI_REG_CTRL_STATE      (1 << 0)    // SPI controller in BUSY state if 1, IDLE if 0
#define SPI_REG_CLK_DIV     2
#define SPI_REG_RAM_LEN     3
#define SPI_REG_CHECKSUM    4
#define SPI_REG_RAM_FIFO    7
#define SPI_REG_RAM_FROM    8
#define SPI_REG_RAM_TO      15


typedef union {
    struct {
        uint32_t busy : 1;
        uint32_t rsv1 : 1;
        uint32_t rsv2 : 1;
        uint32_t csel : 1;   // 0 for TF Card, 1 for reserved
        uint32_t cend : 1;   // De-assert chip select signal (high)
        uint32_t cstart : 1; // Assert the chip select (low)
        uint32_t reset : 1;  // Reset the controller
        uint32_t start : 1;  // Start SPI transaction
    };
    uint32_t raw;
} zvb_spi_ctrl_t;


typedef struct {
    uint8_t data[SPI_RAM_LEN];
    uint8_t idx;
} zvb_spi_ram_t;


/**
 * @brief States for the FSM representing the TF Card
 */
typedef enum {
    TF_WAIT_IDLE,
    TF_IDLE,
    TF_CMD55_RECEIVED,
    TF_READ_BLOCK,
    TF_WRITE_BLOCK_WAIT_TOK,
    TF_WRITE_BLOCK,
    TF_WRITE_BLOCK_SEND_RESP,
} zvb_tf_state_t;


static inline int zvb_tf_is_write(zvb_tf_state_t st) {
    return st == TF_WRITE_BLOCK_WAIT_TOK ||
           st == TF_WRITE_BLOCK ||
           st == TF_WRITE_BLOCK_SEND_RESP;
}


/**
 * @brief Structure representing a TF card
 */
typedef struct {
    zvb_tf_state_t  state;
    FILE*           img;
    size_t          img_size;
    /* Managed by the SPI controller */
    uint8_t         reply[1024];
    int             reply_idx;
    int             reply_len;
} zvb_tf_t;


/**
 * @brief Type for the text controller
 */
typedef struct {
    uint8_t         clk_div;
    uint8_t         ram_len;
    zvb_spi_ram_t   ram_rd;
    zvb_spi_ram_t   ram_wr;
    /* When 1, the TF chip select line is asserted */
    uint8_t         tf_cs;
    zvb_tf_t        tf;
} zvb_spi_t;


/**
 * @brief Initialize the SPI, must be called before using it.
 */
void zvb_spi_init(zvb_spi_t* text);


/**
 * @brief Function to call when a write occurs on the SPI I/O controller.
 *
 * @param addr Address relative to the SPI address space.
 * @param data Byte to write in the text
 */
void zvb_spi_write(zvb_spi_t* spi, uint32_t addr, uint8_t value);


/**
 * @brief Function to call when a read occurs on the SPI I/O controller.
 *
 * @param addr Address relative to the SPI address space.
 */
uint8_t zvb_spi_read(zvb_spi_t* spi, uint32_t addr);


/**
 * @brief Load an image for the TF card
 *
 * @return 0 on success, 1 in case of error
 */
int zvb_spi_load_tf_image(zvb_spi_t* spi, const char* filename);
