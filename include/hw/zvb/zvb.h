#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hw/device.h"
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

/**
 * @file Emulation for the Zeal 8-bit VideoBoard
 */

#define ZVB_MAX_RES_WIDTH   640
#define ZVB_MAX_RES_HEIGHT  480

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
#define STATE_IDLE          0     // Raster in non-vblank area
#define STATE_VBLANK        1
#define STATE_COUNT         2


/**
 * @brief Macros listing of all the objects in the shaders
 */
#define TEXT_SHADER_VIDMODE_IDX     0
#define TEXT_SHADER_TILEMAPS_IDX    1
#define TEXT_SHADER_FONT_IDX        2
#define TEXT_SHADER_PALETTE_IDX     3
#define TEXT_SHADER_CURPOS_IDX      4
#define TEXT_SHADER_CURCOLOR_IDX    5
#define TEXT_SHADER_CURCHAR_IDX     6
#define TEXT_SHADER_TSCROLL_IDX     7

#define TEXT_SHADER_OBJ_COUNT       8


#define GFX_SHADER_VIDMODE_IDX      0
#define GFX_SHADER_TILEMAPS_IDX     1
#define GFX_SHADER_TILESET_IDX      2
#define GFX_SHADER_SPRITES_IDX      3
#define GFX_SHADER_SCROLL0_IDX      4
#define GFX_SHADER_SCROLL1_IDX      5
#define GFX_SHADER_PALETTE_IDX      6

#define GFX_SHADER_OBJ_COUNT        7

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
    uint32_t l0_latch;
    uint32_t l1_latch;
    uint32_t l0_scroll_x;
    uint32_t l0_scroll_y;
    uint32_t l1_scroll_x;
    uint32_t l1_scroll_y;
} zvb_ctrl_t;


typedef enum {
    SHADER_TEXT = 0,
    SHADER_GFX,
    SHADER_BITMAP,
    SHADERS_COUNT,
} zvb_shaders_t;

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

    /* Internally used to make the shader work on the whole screen */
    Shader           shaders[SHADERS_COUNT];
    RenderTexture    tex_dummy;
    /* Array containing the indexes of our objects in the shaders */
    int              text_shader_idx[TEXT_SHADER_OBJ_COUNT];
    int              gfx_shader_idx[GFX_SHADER_OBJ_COUNT];
    int              bitmap_shader_idx[GFX_SHADER_OBJ_COUNT];

    /* Internal values */
    zvb_status_t     status;
    zvb_ctrl_t       ctrl;
    bool             screen_enabled;
    uint8_t          io_bank;
    uint8_t          scratch[4];
    int              state; // Any of the STATE_* macros
    long             tstates_counter;
    bool             need_render;
    /* When rendering to the screen directly, Y must be flipped,
     * But when rendering to a texture (debugger UI), it must not be*/
    bool             flipped_y;
} zvb_t;


/**
 * @brief Initialize the video board
 *
 * @param zvb Context to fill and return
 * @param flipped_y Whether to render the screen mirrored in Y axis
 */
int zvb_init(zvb_t* zvb, bool flipped_y, const memory_op_t* ops);


/**
 * @brief Function to call to let the video board be aware of how many
 * interrupts have elapsed.
 */
void zvb_tick(zvb_t* zvb, const int tstates);


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


/**
 * @brief Used for debugging purpose to show the current rendering when the CPU is stopped
 */
void zvb_force_render(zvb_t* zvb);


/**
 * @brief Deinitialize the video board, closing the window and unloading all assets
 */
void zvb_deinit(zvb_t* zvb);
