#pragma once

#include <stdint.h>
#include "hw/device.h"
#include "hw/zvb/zvb_font.h"
#include "hw/zvb/zvb_palette.h"
#include "hw/zvb/zvb_tilemap.h"
#include "hw/zvb/zvb_text.h"

/**
 * @file Emulation for the Zeal 8-bit VideoBoard
 */


/**
 * @brief Macros for the I/O registers
 */
#define ZVB_IO_BANK_REG     0x0e
#define ZVB_MEM_START_REG   0x0f
#define ZVB_IO_CONF_START   0x10
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


typedef struct {
    device_t         parent;
    zvb_video_mode_t mode;
    zvb_tilemap_t    layers;
    zvb_font_t       font;
    zvb_palette_t    palette;

    /* I/O controllers */
    zvb_text_t       text;

    /* Internally used to make the shader work on the whole screen */
    Shader           text_shader;
    RenderTexture    tex_dummy;

    /* Internal values */
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
