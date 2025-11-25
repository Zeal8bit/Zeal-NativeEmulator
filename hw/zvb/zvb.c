/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "raylib.h"
#include "utils/log.h"
#include "utils/helpers.h"
#include "utils/paths.h"
#include "hw/memory_op.h"
#include "hw/zvb/zvb.h"
#include "hw/zvb/zvb_render.h"
#include "hw/zvb/default_font.h"

#define BENCHMARK           1


#define ZVB_IO_SIZE         (3 * 16)
/* The video board occupies 128KB of memory, but the VRAM is not that big  */
#define ZVB_MEM_SIZE        (128 * 1024)

/* Mapping for all the different types of memory, relative to the VRAM */
#define LAYER0_ADDR_START         (0x00000U)
#define LAYER0_ADDR_END           (0x00C80U)

#define PALETTE_ADDR_START        (0x00E00U)
#define PALETTE_ADDR_END          (0x01000U)

#define LAYER1_ADDR_START         (0x01000U)
#define LAYER1_ADDR_END           (0x01C80U)

#define SPRITES_ADDR_START        (0x02800U)
#define SPRITES_ADDR_END          (0x02C00U)

#define FONT_ADDR_START           (0x03000U)
#define FONT_ADDR_END             (0x03C00U)

#define TILESET_ADDR_START        (0x10000U)
#define TILESET_ADDR_END          (0x20000U)

/**
 * @brief Calculate the size (width or height) counting a grid
 */
#define SIZE_WITH_GRID(TILESIZE, TILECOUNT)    ((((TILESIZE)+1)*TILECOUNT)+1)

/**
 * @brief Helper for checking a range, END not being included!
 */
#define IN_RANGE(START, END, VAL)   ((START) <= (VAL) && (VAL) < (END))


static void zvb_reset(device_t* dev);

static const long s_tstates_remaining[STATE_COUNT] = {
    /* The raster spends 15.253 ms in the visible area */
    [STATE_IDLE]         = US_TO_TSTATES(15253),
    /* The raster stays in V-Blank during 1.430ms  */
    [STATE_VBLANK]       = US_TO_TSTATES(1430),
};


static uint8_t zvb_mem_read(device_t* dev, uint32_t addr)
{
    zvb_t* zvb = (zvb_t*) dev;
    /* Prevent a compilation warning, since LAYER0_ADDR_START is 0 */
    if (addr < LAYER0_ADDR_END) {
        return zvb_tilemap_read(&zvb->layers, 0, addr);
    } else if (IN_RANGE(LAYER1_ADDR_START, LAYER1_ADDR_END, addr)) {
        return zvb_tilemap_read(&zvb->layers, 1, addr - LAYER1_ADDR_START);
    } else if(IN_RANGE(PALETTE_ADDR_START, PALETTE_ADDR_END, addr)) {
        return zvb_palette_read(&zvb->palette, addr - PALETTE_ADDR_START);
    } else if(IN_RANGE(SPRITES_ADDR_START, SPRITES_ADDR_END, addr)) {
        return zvb_sprites_read(&zvb->sprites, addr - SPRITES_ADDR_START);
    } else if(IN_RANGE(FONT_ADDR_START, FONT_ADDR_END, addr)) {
        return zvb_font_read(&zvb->font, addr - FONT_ADDR_START);
    } else if(IN_RANGE(TILESET_ADDR_START, TILESET_ADDR_END, addr)) {
        return zvb_tileset_read(&zvb->tileset, addr - TILESET_ADDR_START);
    }
    return 0;
}


static void zvb_mem_write(device_t* dev, uint32_t addr, uint8_t data)
{
    zvb_t* zvb = (zvb_t*) dev;
    /* Prevent a compilation warning, since LAYER0_ADDR_START is 0 */
    if (addr < LAYER0_ADDR_END) {
        zvb_tilemap_write(&zvb->layers, 0, addr, data);
    } else if (IN_RANGE(LAYER1_ADDR_START, LAYER1_ADDR_END, addr)) {
        zvb_tilemap_write(&zvb->layers, 1, addr - LAYER1_ADDR_START, data);
    } else if(IN_RANGE(PALETTE_ADDR_START, PALETTE_ADDR_END, addr)) {
        zvb_palette_write(&zvb->palette, addr - PALETTE_ADDR_START, data);
    } else if(IN_RANGE(SPRITES_ADDR_START, SPRITES_ADDR_END, addr)) {
        zvb_sprites_write(&zvb->sprites, addr - SPRITES_ADDR_START, data);
    } else if(IN_RANGE(FONT_ADDR_START, FONT_ADDR_END, addr)) {
        zvb_font_write(&zvb->font, addr - FONT_ADDR_START, data);
    } else if(IN_RANGE(TILESET_ADDR_START, TILESET_ADDR_END, addr)) {
        zvb_tileset_write(&zvb->tileset, addr - TILESET_ADDR_START, data);
    }
}


static uint8_t zvb_io_read_control(zvb_t* zvb, uint32_t addr)
{
    switch(addr) {
        case ZVB_IO_CONFIG_L0_SCR_Y_LOW:    return (zvb->ctrl.l0_scroll_y >> 0) & 0xff;
        case ZVB_IO_CONFIG_L0_SCR_Y_HIGH:   return (zvb->ctrl.l0_scroll_y >> 8) & 0xff;
        case ZVB_IO_CONFIG_L0_SCR_X_LOW:    return (zvb->ctrl.l0_scroll_x >> 0) & 0xff;
        case ZVB_IO_CONFIG_L0_SCR_X_HIGH:   return (zvb->ctrl.l0_scroll_x >> 8) & 0xff;

        case ZVB_IO_CONFIG_L1_SCR_Y_LOW:    return (zvb->ctrl.l1_scroll_y >> 0) & 0xff;
        case ZVB_IO_CONFIG_L1_SCR_Y_HIGH:   return (zvb->ctrl.l1_scroll_y >> 8) & 0xff;
        case ZVB_IO_CONFIG_L1_SCR_X_LOW:    return (zvb->ctrl.l1_scroll_x >> 0) & 0xff;
        case ZVB_IO_CONFIG_L1_SCR_X_HIGH:   return (zvb->ctrl.l1_scroll_x >> 8) & 0xff;

        case ZVB_IO_CONFIG_MODE_REG:        return zvb->mode;
        case ZVB_IO_CONFIG_STATUS_REG:      return zvb->status.raw;
        default:
            log_err_printf("[ZVB][CTRL] Unknwon register %x\n", addr);
            break;
    }

    return 0;
}


static uint8_t zvb_io_read(device_t* dev, uint32_t addr)
{
    zvb_t* zvb = (zvb_t*) dev;

    /* Video Board configuration goes from 0x00 to 0x0F included */
    if (addr == ZVB_IO_REV_REG)  {
        return ZVB_EMULATED_REV;
    } else if (addr == ZVB_IO_MINOR_REG) {
        return ZVB_EMULATED_MINOR;
    } else if (addr == ZVB_IO_MAJOR_REG) {
        return ZVB_EMULATED_MAJOR;
    } else if (addr >= ZVB_IO_SCRAT0_REG && addr <= ZVB_IO_SCRAT3_REG) {
        return zvb->scratch[addr - ZVB_IO_SCRAT0_REG];
    } else if (addr == ZVB_IO_BANK_REG) {
        return zvb->io_bank;
    } else if (addr == ZVB_MEM_START_REG) {
    } else if (addr >= ZVB_IO_CONF_START && addr < ZVB_IO_CONF_END) {
        const uint32_t subaddr = addr - ZVB_IO_CONF_START;
        return zvb_io_read_control(zvb, subaddr);
    } else if (addr >= ZVB_IO_BANK_START && addr < ZVB_IO_BANK_END) {
        const uint32_t subaddr = addr - ZVB_IO_BANK_START;
        switch (zvb->io_bank) {
            case ZVB_IO_MAPPING_TEXT:  return zvb_text_read(&zvb->text, subaddr);
            case ZVB_IO_MAPPING_SPI:   return zvb_spi_read(&zvb->spi, subaddr);
            case ZVB_IO_MAPPING_CRC:   return zvb_crc32_read(&zvb->peri_crc32, subaddr);
            case ZVB_IO_MAPPING_SOUND: return zvb_sound_read(&zvb->sound, subaddr);
            case ZVB_IO_MAPPING_DMA:   return zvb_dma_read(&zvb->dma, subaddr);
            default: break;
        }
    }

    return 0;
}


static void zvb_io_write_control(zvb_t* zvb, uint32_t addr, uint8_t value)
{
    /* We may need to interpret the data as a status below */
    const zvb_status_t status = { .raw = value };

    switch(addr) {
        case ZVB_IO_CONFIG_L0_SCR_Y_LOW:
        case ZVB_IO_CONFIG_L0_SCR_X_LOW:
            zvb->ctrl.l0_latch = value;
            break;
        case ZVB_IO_CONFIG_L0_SCR_Y_HIGH:
            zvb->ctrl.l0_scroll_y = (value << 8) + zvb->ctrl.l0_latch;
            break;
        case ZVB_IO_CONFIG_L0_SCR_X_HIGH:
            zvb->ctrl.l0_scroll_x = (value << 8) + zvb->ctrl.l0_latch;
            break;

        case ZVB_IO_CONFIG_L1_SCR_Y_LOW:
        case ZVB_IO_CONFIG_L1_SCR_X_LOW:
            zvb->ctrl.l1_latch = value;
            break;
        case ZVB_IO_CONFIG_L1_SCR_Y_HIGH:
            zvb->ctrl.l1_scroll_y = (value << 8) + zvb->ctrl.l1_latch;
            break;
        case ZVB_IO_CONFIG_L1_SCR_X_HIGH:
            zvb->ctrl.l1_scroll_x = (value << 8) + zvb->ctrl.l1_latch;
            break;

        case ZVB_IO_CONFIG_MODE_REG:
            zvb->mode = value;
            break;
        case ZVB_IO_CONFIG_STATUS_REG:
            zvb->status.vid_ena = status.vid_ena;
            break;
        default:
            log_err_printf("[ZVB][CTRL] Unknown register %x\n", addr);
            break;
    }
}


static void zvb_io_write(device_t* dev, uint32_t addr, uint8_t data)
{
    zvb_t* zvb = (zvb_t*) dev;
    /* Video Board configuration goes from 0x00 to 0x0F included */
    if (addr >= ZVB_IO_SCRAT0_REG && addr <= ZVB_IO_SCRAT3_REG) {
        zvb->scratch[addr - ZVB_IO_SCRAT0_REG] = data;
    } else if (addr == ZVB_IO_BANK_REG) {
        zvb->io_bank = data;
    } else if (addr == ZVB_MEM_START_REG) {
        log_err_printf("[WARNING] zvb memory mapping register is not supported\n");
    } else if (addr >= ZVB_IO_CONF_START && addr < ZVB_IO_CONF_END) {
        const uint32_t subaddr = addr - ZVB_IO_CONF_START;
        zvb_io_write_control(zvb, subaddr, data);
    } else if (addr >= ZVB_IO_BANK_START && addr < ZVB_IO_BANK_END) {
        const uint32_t subaddr = addr - ZVB_IO_BANK_START;
        switch (zvb->io_bank) {
            case ZVB_IO_MAPPING_TEXT:
                zvb_text_write(&zvb->text, subaddr, data, &zvb->layers);
                break;
            case ZVB_IO_MAPPING_SPI:
                zvb_spi_write(&zvb->spi, subaddr, data);
                break;
            case ZVB_IO_MAPPING_CRC:
                zvb_crc32_write(&zvb->peri_crc32, subaddr, data);
                break;
            case ZVB_IO_MAPPING_SOUND:
                zvb_sound_write(&zvb->sound, subaddr, data);
                break;
            case ZVB_IO_MAPPING_DMA:
                zvb_dma_write(&zvb->dma, subaddr, data);
                break;
            default:
                break;
        }
    }
}


int zvb_init(zvb_t* dev, bool flipped_y, const memory_op_t* ops)
{
    if (dev == NULL) {
        return 1;
    }

    /* Initialize the structure and register it on both the memory and I/O buses */
    memset(dev, 0, sizeof(zvb_t));
    device_init_mem(DEVICE(dev), "zvb_dev", zvb_mem_read, zvb_mem_write, ZVB_MEM_SIZE);
    device_init_io(DEVICE(dev),  "zvb_dev", zvb_io_read, zvb_io_write, ZVB_IO_SIZE);
    device_register_reset(DEVICE(dev), zvb_reset);
    dev->mode = MODE_DEFAULT;

    zvb_palette_init(&dev->palette);
    zvb_font_init(&dev->font);
    zvb_tilemap_init(&dev->layers);
    zvb_tileset_init(&dev->tileset);
    zvb_text_init(&dev->text);
    zvb_sprites_init(&dev->sprites);
    zvb_spi_init(&dev->spi);
    zvb_crc32_init(&dev->peri_crc32);
    zvb_sound_init(&dev->sound);
    zvb_dma_init(&dev->dma, ops);

    dev->tex_dummy = LoadRenderTexture(ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT);
#if CONFIG_ENABLE_DEBUGGER
    dev->debug_tex[DBG_TILEMAP_LAYER0]  = LoadRenderTexture(ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_HEIGHT);
    dev->debug_tex[DBG_TILEMAP_LAYER1]  = LoadRenderTexture(ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_HEIGHT);
    /* Count the grid in the width. For the tileset, use a 16x32 tiles size */
    dev->debug_tex[DBG_TILESET] = LoadRenderTexture(SIZE_WITH_GRID(16, 16), SIZE_WITH_GRID(16, 32));
    dev->debug_tex[DBG_PALETTE] = LoadRenderTexture(SIZE_WITH_GRID(16, 16), SIZE_WITH_GRID(16, 16));
    dev->debug_tex[DBG_FONT]    = LoadRenderTexture(SIZE_WITH_GRID(8, 16),  SIZE_WITH_GRID(12, 16));
    /* For the debugger */
    dev->flipped_y = flipped_y;
#else
    /* Unused, prevent a warning */
    (void) flipped_y;
#endif
    zvb_render_init(&dev->render, dev);

    /* Set the state to STATE_IDLE, waiting for the next event */
    dev->state = STATE_IDLE;
    dev->tstates_counter = s_tstates_remaining[dev->state];

    /* Enable the screen by default */
    dev->status.vid_ena = 1;
    dev->need_render = false;

    return 0;
}

static void zvb_reset(device_t* dev)
{
    zvb_t* zvb = (zvb_t*) dev;
    zvb_text_reset(&zvb->text);
    zvb_spi_reset(&zvb->spi);
    zvb_crc32_reset(&zvb->peri_crc32);
    zvb_sound_reset(&zvb->sound);
    zvb_dma_reset(&zvb->dma);
}


/**
 * @brief Render the screen when `vid_ena` is set (screen disabled)
 */
static void zvb_render_disabled_mode(zvb_t* zvb)
{
    (void) zvb;
    ClearBackground(BLACK);
}


/**
 * @brief Render the screen in text mode (80x40 and 40x20)
 */
static void zvb_prepare_render_text_mode(zvb_t* zvb)
{
    zvb_palette_update(&zvb->palette);
    zvb_font_update(&zvb->font);
    zvb_tilemap_update(&zvb->layers);
}



static void zvb_prepare_render_gfx_mode(zvb_t* zvb)
{
    zvb_palette_update(&zvb->palette);
    zvb_tileset_update(&zvb->tileset);
    zvb_tilemap_update(&zvb->layers);
    zvb_sprites_update(&zvb->sprites);
}

static void zvb_prepare_render_bitmap_mode(zvb_t* zvb)
{
    zvb_palette_update(&zvb->palette);
    zvb_tileset_update(&zvb->tileset);
}


/* Prepare the rendering by updating the udnerneath textures */
bool zvb_prepare_render(zvb_t* zvb)
{
    /* Only update the texture if we are going to render anything */
    if (!zvb->need_render) {
        return false;
    }

    switch (zvb->mode) {
        case MODE_TEXT_640:
        case MODE_TEXT_320:
            zvb_prepare_render_text_mode(zvb);
            break;

        case MODE_BITMAP_256:
        case MODE_BITMAP_320:
            zvb_prepare_render_bitmap_mode(zvb);
            break;

        default:
            zvb_prepare_render_gfx_mode(zvb);
            break;
    }

    return true;
}

#if CONFIG_ENABLE_DEBUGGER
void zvb_render_debug_textures(zvb_t* zvb)
{
    switch (zvb->mode) {
        case MODE_TEXT_640:
        case MODE_TEXT_320:
            zvb_render_debug_text_mode(&zvb->render, zvb);
            break;

        case MODE_BITMAP_256:
        case MODE_BITMAP_320:
            // zvb_render_debug_bitmap_mode(zvb);
            break;

        default:
            zvb_render_debug_gfx_mode(&zvb->render, zvb);
            break;
    }
}
#endif

void zvb_render(zvb_t* zvb)
{
    if (zvb->need_render == false) {
        return;
    }

    zvb->need_render = false;

#if BENCHMARK
    double startTime = GetTime();
    static double average = 0;
    static int counter = 0;
#endif

    if (zvb->status.vid_ena) {
        switch (zvb->mode) {
            case MODE_TEXT_640:
            case MODE_TEXT_320:
                zvb_render_text_mode(&zvb->render, zvb);
                break;

            case MODE_BITMAP_256:
            case MODE_BITMAP_320:
                zvb_render_bitmap_mode(&zvb->render, zvb);
                break;

            default:
                zvb_render_gfx_mode(&zvb->render, zvb);
                break;
        }
    } else {
        zvb_render_disabled_mode(zvb);
    }

#if BENCHMARK
    double endTime = GetTime();
    double elapsedTime = endTime - startTime;
    average += elapsedTime;
    if (++counter == 60) {
        log_printf("Time taken : %f ms\n", (average / 60) * 1000);
        counter = 0;
        average = 0;
    }
#endif
}


void zvb_force_render(zvb_t* zvb)
{
    zvb->need_render = true;
    zvb_prepare_render(zvb);
    zvb_render(zvb);
}


void zvb_tick(zvb_t* zvb, const int tstates)
{
    zvb->tstates_counter -= tstates;

    if (zvb->tstates_counter <= 0) {
        /* Go to the next state */
        zvb->state = (zvb->state + 1) % STATE_COUNT;
        zvb->tstates_counter = s_tstates_remaining[zvb->state];
        /* If the new state is V-blank (i.e. we reached blank), render the screen */
        if (zvb->state == STATE_VBLANK) {
            zvb->status.v_blank = 1;
            zvb->need_render = true;
        } else {
            zvb->status.v_blank = 0;
        }
    }
}


void zvb_deinit(zvb_t* zvb)
{
    UnloadRenderTexture(zvb->tex_dummy);
#if CONFIG_ENABLE_DEBUGGER
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        UnloadRenderTexture(zvb->debug_tex[i]);
    }
#endif
}
