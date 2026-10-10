/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hw/device.h"
#include "hw/mmu.h"
#include "utils/vtimer.h"
#include "hw/zvb/zvb_font.h"
#include "hw/zvb/zvb_palette.h"
#include "hw/zvb/zvb_tilemap.h"
#include "hw/zvb/zvb_tileset.h"
#include "hw/zvb/zvb_text.h"
#include "hw/zvb/zvb_sprites.h"
#include "hw/zvb/zvb_spi.h"
#include "hw/zvb/zvb_crc32.h"
#include "hw/zvb/zvb_sound.h"
#include "hw/zvb/zvb_dma.h"
#include "debugger/debugger_types.h"

#if ZVB_BLITTER_SHADER
#include "hw/zvb/blitter/shader.h"
#elif ZVB_BLITTER_SOFTWARE
#include "hw/zvb/blitter/software.h"
#endif


/**
 * @file Emulation for the Zeal 8-bit VideoBoard
 */
#define ZVB_EMULATED_REV    0
#define ZVB_EMULATED_MINOR  0
#define ZVB_EMULATED_MAJOR  1

#define ZVB_MAX_RES_WIDTH   640
#define ZVB_MAX_RES_HEIGHT  480

/**
 * @brief Width and height for the debug textures, account for the grid of 1px
 */
#define ZVB_DBG_RES_WIDTH   1361    // 80 tiles * (16px + 1px grid) + 1px grid right border
#define ZVB_DBG_RES_HEIGHT  681     // 40 tiles * (16px + 1px grid) + 1px grid bottom border

/**
 * @brief Macros for the I/O registers
 */
#define ZVB_IO_REV_REG      0x00
#define ZVB_IO_MINOR_REG    0x01
#define ZVB_IO_MAJOR_REG    0x02
#define ZVB_IO_SCRAT0_REG   0x08
#define ZVB_IO_SCRAT1_REG   0x09
#define ZVB_IO_SCRAT2_REG   0x0a
#define ZVB_IO_SCRAT3_REG   0x0b
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
#define ZVB_IO_MAPPING_DMA      4


/**
 * @brief Macros for the raster state
 */
#define STATE_RENDERING     0     // Raster in non-vblank area
#define STATE_HBLANK        1     // Raster in horizontal blanking area
#define STATE_COUNT         2


typedef enum {
    MODE_TEXT_640     = 0,
    MODE_TEXT_320     = 1,
    MODE_BITMAP_256   = 2,
    MODE_BITMAP_320   = 3,
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
    uint8_t  vpos_latch;
    uint8_t  l0_latch;
    uint8_t  l1_latch;
    uint32_t l0_scroll_x;
    uint32_t l0_scroll_y;
    uint32_t l1_scroll_x;
    uint32_t l1_scroll_y;
} zvb_ctrl_t;



typedef struct {
    bool rendering_enabled;
} zvb_config_t;


/** @brief One 32-bit RGBA pixel, the format every debug view is rendered in. */
typedef struct {
    uint8_t r, g, b, a;
} zvb_rgba_t;

#define ZVB_RGBA_BLANK ((zvb_rgba_t){ 0, 0, 0, 0 })
#define ZVB_RGBA_BLACK ((zvb_rgba_t){ 0, 0, 0, 255 })
#define ZVB_RGBA_WHITE ((zvb_rgba_t){ 255, 255, 255, 255 })

/**
 * @brief One VRAM debug view.
 *
 * The CPU debug renderer fills @c pixels directly and the debugger reads them from
 * there, so a view owns no GPU resource at all.
 */
typedef struct {
    zvb_rgba_t* pixels; /* width * height RGBA8888 pixels */
    int width;
    int height;
} zvb_debug_image_t;

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
    zvb_spi_t        spi;
    zvb_crc32_t      peri_crc32;
    zvb_sound_t      sound;
    zvb_dma_t        dma;

    /* Blitter/renderer related */
    zvb_blitter_t blitter;
#if CONFIG_ENABLE_DEBUGGER
    /* CPU pixel buffer for each VRAM debug view */
    zvb_debug_image_t debug_img[DBG_VIEW_TOTAL];
#endif

    /* Internal values */
    zvb_status_t     status;
    zvb_ctrl_t       ctrl;
    bool             screen_enabled;
    uint8_t          io_bank;
    uint8_t          scratch[4];

    /* Raster FSM */
    int              state; // Any of the STATE_* macros
    vtimer_node_t    timer;
    int              current_scanline;
    bool             need_render;
    bool             rendering_enabled;
} zvb_t;


static inline bool zvb_is_bitmap_mode(const zvb_t* zvb)
{
    return zvb->mode == MODE_BITMAP_256 || zvb->mode == MODE_BITMAP_320;
}

static inline bool zvb_is_gfx_mode(const zvb_t* zvb)
{
    return zvb->mode == MODE_GFX_640_8BIT ||
           zvb->mode == MODE_GFX_320_8BIT ||
           zvb->mode == MODE_GFX_640_4BIT ||
           zvb->mode == MODE_GFX_320_4BIT;
}

static inline bool zvb_is_text_mode(const zvb_t* zvb)
{
    return zvb->mode == MODE_TEXT_640 || zvb->mode == MODE_TEXT_320;
}

/**
 * @brief Initialize the video board
 *
 * @param zvb Context to fill and return
 * @param config Initialization options for the video board
 */
int zvb_init(zvb_t* zvb, const zvb_config_t* config, mmu_t* mmu);


/**
 * @brief Function to call to let the video board be aware of how many
 * interrupts have elapsed.
 */
/**
 * @brief Prepare the rendering, this will update the textures and images.
 * Must be called before `zvb_render`!
 *
 * @returns true if ZVB is ready to render (display reached refresh state), false else
 */
bool zvb_prepare_render(zvb_t* zvb);


/**
 * @brief Perform any rendering operation if necessary
 */
void zvb_render(zvb_t* zvb);
#if CONFIG_PROFILE_RENDER
void zvb_profile_frame(double elapsed_seconds);
#endif


/**
 * @brief Used for debugging purpose to show the current rendering when the CPU is stopped
 */
void zvb_force_render(zvb_t* zvb);


/**
 * @brief Deinitialize the video board, closing the window and unloading all assets
 */
void zvb_deinit(zvb_t* zvb);


#if ZVB_BLITTER_SHADER
/**
 * @brief Get the texture containing the rendered frame, ready to draw to screen.
 *
 * Only the GPU blitter has one; a host that can present a CPU buffer should use
 * zvb_output_pixels() instead.
 */
static inline Texture zvb_output_texture(zvb_t* zvb)
{
    return zvb->blitter.main_texture.texture;
}
#endif


/**
 * @brief Get the rendered frame as a CPU pixel buffer, or NULL when the blitter keeps
 *        its output on the GPU.
 *
 * A host that can present a CPU buffer should prefer this: reading the frame back from
 * the GPU costs a full-frame copy every time it is needed.
 */
static inline const void* zvb_output_pixels(zvb_t* zvb, int* width, int* height, int* pitch,
                                            bool* rgb565)
{
#if ZVB_BLITTER_SOFTWARE
    if (width) {
        *width = ZVB_MAX_RES_WIDTH;
    }
    if (height) {
        *height = ZVB_MAX_RES_HEIGHT;
    }
    if (pitch) {
        *pitch = ZVB_MAX_RES_WIDTH * 2;
    }
    if (rgb565) {
        *rgb565 = true;
    }
    return zvb->blitter.framebuffer;
#else
    (void)zvb;
    (void)width;
    (void)height;
    (void)pitch;
    (void)rgb565;
    return NULL;
#endif
}


#if CONFIG_ENABLE_DEBUGGER
/**
 * @brief Render the current VRAM state of one debug view, must be called after `render` function
 */
void zvb_render_debug_textures(zvb_t* zvb, dbg_vram_t view);
#endif
