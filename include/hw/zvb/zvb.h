#pragma once

#include <stdint.h>
#include "hw/device.h"
#include "hw/zvb/zvb_font.h"
#include "hw/zvb/zvb_palette.h"
#include "hw/zvb/zvb_tilemap.h"
#include "hw/zvb/zvb_tileset.h"
#include "hw/zvb/zvb_text.h"
#include "hw/zvb/zvb_sprites.h"

/**
 * @file Emulation for the Zeal 8-bit VideoBoard
 */


/**
 * @brief Macros for the I/O registers
 */
#define ZVB_IO_BANK_REG     0x0e
#define ZVB_MEM_START_REG   0x0f
#define ZVB_IO_CONF_START   0x10
    #define ZVB_IO_CONFIG_VPOS_LOW      0x00
    #define ZVB_IO_CONFIG_VPOS_HIGH     0x01
    #define ZVB_IO_CONFIG_HPOS_LOW      0x02
    #define ZVB_IO_CONFIG_HPOS_HIGH     0x03
    #define ZVB_IO_CONFIG_L0_SCR_Y_LOW  0x04
    #define ZVB_IO_CONFIG_L0_SCR_Y_HIGH 0x05
    #define ZVB_IO_CONFIG_L0_SCR_X_LOW  0x06
    #define ZVB_IO_CONFIG_L0_SCR_X_HIGH 0x07
    #define ZVB_IO_CONFIG_L1_SCR_Y_LOW  0x08
    #define ZVB_IO_CONFIG_L1_SCR_Y_HIGH 0x09
    #define ZVB_IO_CONFIG_L1_SCR_X_LOW  0x0a
    #define ZVB_IO_CONFIG_L1_SCR_X_HIGH 0x0b
    #define ZVB_IO_CONFIG_MODE_REG      0x0c
    #define ZVB_IO_CONFIG_STATUS_REG    0x0d
#define ZVB_IO_CONF_END     0x20
#define ZVB_IO_BANK_START   0x20
#define ZVB_IO_BANK_END     0x30

/**
 * @brief Each I/O controller can be mapped in the I/O bank, get them an index
 */
#define ZVB_IO_MAPPING_TEXT     0
#define ZVB_IO_MAPPING_SPI      1
#define ZVB_IO_MAPPING_CRC      2
#define ZVB_IO_MAPPING_SOUND    3


/**
 * @brief Macros for the raster state
 */
#define STATE_IDLE          0     // Raster in non-vblank area
#define STATE_VBLANK        1
#define STATE_COUNT         2


typedef enum {
    MODE_TEXT_640     = 0,
    MODE_TEXT_320     = 1,
    MODE_GFX_640_8BIT = 4,
    MODE_GFX_320_8BIT = 5,
    MODE_GFX_640_4BIT = 6,
    MODE_GFX_320_4BIT = 7,
    MODE_LAST         = MODE_GFX_320_4BIT,
    MODE_DEFAULT      = MODE_TEXT_640,
} zvb_video_mode_t;


/**
 * @brief Status register in the configuration "bank"
 */
typedef union {
    struct {
        uint8_t h_blank : 1;
        uint8_t v_blank : 1;
        uint8_t rsvd    : 5;
        uint8_t vid_ena : 1;
    };
    uint8_t raw;
} zvb_status_t;


typedef struct {
    uint32_t l0_latch;
    uint32_t l1_latch;
    uint32_t l0_scroll_x;
    uint32_t l0_scroll_y;
    uint32_t l1_scroll_x;
    uint32_t l1_scroll_y;
} zvb_ctrl_t;


typedef struct {
    device_t         parent;
    zvb_video_mode_t mode;
    zvb_tilemap_t    layers;
    zvb_font_t       font;
    zvb_tileset_t    tileset;
    zvb_palette_t    palette;
    zvb_sprites_t    sprites;

    /* I/O controllers */
    zvb_text_t       text;

    /* Internally used to make the shader work on the whole screen */
    Shader           text_shader;
    Shader           gfx_shader;
    RenderTexture    tex_dummy;

    /* Internal values */
    zvb_status_t     status;
    zvb_ctrl_t       ctrl;
    bool             screen_enabled;
    uint8_t          io_bank;
    int              state; // Any of the STATE_* macros
    long             tstates_counter;
} zvb_t;


/**
 * @brief Initialize the video board
 */
int zvb_init(zvb_t* zvb);


/**
 * @brief Function to call to let the video board be aware of how many
 * interrupts have elapsed.
 */
void zvb_tick(zvb_t* zvb, const int tstates);


/**
 * @brief Check whether the window is still opened (i.e. should not be closed)
 */
bool zvb_window_opened(zvb_t* zvb);


/**
 * @brief Deinitialize the video board, closing the window and unloading all assets
 */
void zvb_deinit(zvb_t* zvb);


/**
 * @brief Used for debugging purpose
 */
void zvb_force_render(zvb_t* zvb);
