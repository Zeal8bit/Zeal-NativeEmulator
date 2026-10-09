/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
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
#include "hw/mmu.h"
#include "hw/zvb/zvb.h"
#include "hw/zvb/default_font.h"
#include "hw/zvb/blitter/blitter.h"

#define ZVB_MEM_CONFIG_START             (0x1fe0U)
#define ZVB_MEM_PERIPHERALS_START        (0x2000U)
#define ZVB_MEM_PERIPHERALS_END          (0x20e0U)
#define ZVB_MEM_PERIPHERAL_SLOT_SIZE     (32)
#define ZVB_REGISTER_MASK                (15)
#define ZVB_DEFAULT_PHYS_BANK            (8)
#define ZVB_MEM_CONFIG_BANK              (-1)
#define ZVB_IO_BANK_MASK                 (63)
#define ZVB_HBLANK_LATCH                 (1)
#define ZVB_VBLANK_LATCH                 (2)
#define ZVB_BLANK_LATCH_MASK             (3)
#define ZVB_SCROLL_Y_HIGH_MASK           (3)
#define ZVB_SCROLL_X_HIGH_MASK           (7)
#define ZVB_SCROLL_Y_WRAP                (640)
#define ZVB_SCROLL_X_WRAP                (1280)
#define ZVB_MASTER_CLOCK_NS             (20)
#define ZVB_VISIBLE_LINE_NS             (ZVB_MAX_RES_WIDTH * ZVB_PIXEL_NS)
#define ZVB_HBLANK_NS                   ((ZVB_TOTAL_PIXELS_PER_LINE - ZVB_MAX_RES_WIDTH) * ZVB_PIXEL_NS)
#define ZVB_PIXEL_NS                     (40)
#define ZVB_TOTAL_PIXELS_PER_LINE        (800)
#define ZVB_TOTAL_SCANLINES              (524)
#define ZVB_HPOS_HIGH_MASK               (7)
#define ZVB_HPOS_LOW_MASK                (255)
#define ZVB_RESET_STATUS                 (0x80)

/**
 * @brief Calculate the size (width or height) counting a grid
 */
#define SIZE_WITH_GRID(TILESIZE, TILECOUNT)    ((((TILESIZE)+1)*TILECOUNT)+1)


#if CONFIG_PROFILE_RENDER
typedef struct {
    double window_start;
    double frame_total;
    double frame_max;
    double zvb_total;
    double zvb_max;
    uint64_t frames;
    uint64_t zvb_calls;
} render_profile_t;

static render_profile_t s_render_profile;
#endif


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
 * @brief Helper for checking a range, END not being included!
 */
#define IN_RANGE(START, END, VAL)   ((START) <= (VAL) && (VAL) < (END))


static void zvb_reset(device_t* dev);
static uint8_t zvb_io_read(device_t* dev, uint32_t addr);
static void zvb_io_write(device_t* dev, uint32_t addr, uint8_t data);
static void zvb_peripheral_write(zvb_t* zvb, uint8_t bank, uint32_t subaddr, uint8_t data);

#if ZVB_BLITTER_SOFTWARE
static void zvb_sync_clocks(zvb_t* zvb, uint64_t target_ns);
static void zvb_schedule_sound(zvb_t* zvb);
static void zvb_sound_next(void* userdata);
#endif

static uint8_t zvb_external_interrupts(const zvb_t* zvb)
{
    return (zvb_sound_interrupt(&zvb->sound) ? ZVB_EXT_INT_SOUND : 0) |
           (zvb_rpu_interrupt(&zvb->rpu) ? ZVB_EXT_INT_RPU : 0) |
           (zvb_timer_interrupt(&zvb->peri_timer) ? ZVB_EXT_INT_TIMER : 0);
}

static bool zvb_gp_interrupt(const zvb_t* zvb)
{
    return zvb_external_interrupts(zvb) || (zvb->status.h_int_ena && (zvb->blank_latches & ZVB_HBLANK_LATCH));
}

static void zvb_update_interrupts(void* userdata)
{
    zvb_t* zvb = userdata;
    if (!zvb->pio) {
        return;
    }
    pio_set_b_pin(zvb->pio, ZVB_PIO_VBLANK_PIN,
                  !(zvb->status.v_int_ena && (zvb->blank_latches & ZVB_VBLANK_LATCH)));
    pio_set_b_pin(zvb->pio, ZVB_PIO_GP_PIN, !zvb_gp_interrupt(zvb));
}

static uint8_t zvb_peripheral_read(zvb_t* zvb, uint8_t bank, uint32_t addr)
{
    switch (bank) {
        case ZVB_IO_MAPPING_TEXT:
            return zvb_text_read(&zvb->text, addr);
        case ZVB_IO_MAPPING_SPI:
            return zvb_spi_read(&zvb->spi, addr);
        case ZVB_IO_MAPPING_CRC:
            return zvb_crc32_read(&zvb->peri_crc32, addr);
        case ZVB_IO_MAPPING_SOUND:
            return zvb_sound_read(&zvb->sound, addr);
        case ZVB_IO_MAPPING_DMA:
            return zvb_dma_read(&zvb->dma, addr);
        case ZVB_IO_MAPPING_RPU:
            return zvb_rpu_read(&zvb->rpu, addr);
        case ZVB_IO_MAPPING_TIMER:
            return zvb_timer_read(&zvb->peri_timer, addr);
        default:
            return 0;
    }
}

/* Decode configuration aliases and banked peripheral slots in one place. */
static inline int zvb_mem_io_address(uint32_t addr, int* bank)
{
    if (IN_RANGE(ZVB_MEM_CONFIG_START, ZVB_MEM_PERIPHERALS_START, addr)) {
        *bank = ZVB_MEM_CONFIG_BANK;
        return addr - ZVB_MEM_CONFIG_START;
    }
    if (IN_RANGE(ZVB_MEM_PERIPHERALS_START, ZVB_MEM_PERIPHERALS_END, addr)) {
        *bank = (addr - ZVB_MEM_PERIPHERALS_START) / ZVB_MEM_PERIPHERAL_SLOT_SIZE;
        return addr & ZVB_REGISTER_MASK;
    }
    return -1;
}


#if !ZVB_BLITTER_SOFTWARE
static const long s_tstates_remaining[STATE_COUNT] = {
    /* The raster spends 25.6us rendering a single scanline */
    [STATE_RENDERING] = 256,
    /* H-Blank lasts 6.4us */
    [STATE_HBLANK]    = 64,
};
#endif


static uint8_t zvb_mem_read(device_t* dev, uint32_t addr)
{
    zvb_t* zvb = (zvb_t*) dev;
#if ZVB_BLITTER_SOFTWARE
    zvb_sync_clocks(zvb, vtimer_now_ns());
#endif
    int bank;
    const int io_addr = zvb_mem_io_address(addr, &bank);
    if (io_addr >= 0) {
        if (bank != ZVB_MEM_CONFIG_BANK) {
            return zvb_peripheral_read(zvb, bank, io_addr);
        }
        return zvb_io_read(dev, io_addr);
    }
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
#if ZVB_BLITTER_SOFTWARE
    zvb_sync_clocks(zvb, vtimer_now_ns());
#endif
    int bank;
    const int io_addr = zvb_mem_io_address(addr, &bank);
    if (io_addr >= 0) {
        if (bank != ZVB_MEM_CONFIG_BANK) {
            zvb_peripheral_write(zvb, bank, io_addr, data);
        } else {
            zvb_io_write(dev, io_addr, data);
        }
        return;
    }
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
        case ZVB_IO_CONFIG_VPOS_LOW:
            zvb->ctrl.vpos_latch = (zvb->current_scanline >> 8);
            return (zvb->current_scanline >> 0) & 0xff;
        case ZVB_IO_CONFIG_VPOS_HIGH:       return zvb->ctrl.vpos_latch;
        case ZVB_IO_CONFIG_HPOS_LOW: {
            uint32_t hpos = zvb->current_hpos;
#if ZVB_BLITTER_SOFTWARE
            hpos = (vtimer_now_ns() - zvb->line_start_ns) / ZVB_PIXEL_NS;
#endif
            zvb->ctrl.hpos_latch = (hpos >> 8) & ZVB_HPOS_HIGH_MASK;
            return hpos & ZVB_HPOS_LOW_MASK;
        }
        case ZVB_IO_CONFIG_HPOS_HIGH:
            return zvb->ctrl.hpos_latch;

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
        case ZVB_IO_CONFIG_INT_STATUS_REG:
            return zvb->blank_latches | (zvb_gp_interrupt(zvb) << 2);
        case ZVB_IO_CONFIG_EXT_STATUS_REG:
            return zvb_external_interrupts(zvb);
        default:
            log_err_printf("[ZVB][CTRL] Unknwon register %x\n", addr);
            break;
    }

    return 0;
}


static uint8_t zvb_io_read(device_t* dev, uint32_t addr)
{
    zvb_t* zvb = (zvb_t*) dev;
#if ZVB_BLITTER_SOFTWARE
    zvb_sync_clocks(zvb, vtimer_now_ns());
#endif

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
        return ZVB_DEFAULT_PHYS_BANK;
    } else if (addr >= ZVB_IO_CONF_START && addr < ZVB_IO_CONF_END) {
        const uint32_t subaddr = addr - ZVB_IO_CONF_START;
        return zvb_io_read_control(zvb, subaddr);
    } else if (addr >= ZVB_IO_BANK_START && addr < ZVB_IO_BANK_END) {
        const uint32_t subaddr = addr - ZVB_IO_BANK_START;
        return zvb_peripheral_read(zvb, zvb->io_bank, subaddr);
    }

    return 0;
}


static void zvb_io_write_control(zvb_t* zvb, uint32_t addr, uint8_t value)
{
    /* We may need to interpret the data as a status below */
    const zvb_status_t status = { .raw = value };

    switch(addr) {
        case ZVB_IO_CONFIG_L0_SCR_Y_LOW:
        case ZVB_IO_CONFIG_L1_SCR_Y_LOW:
            zvb->ctrl.scroll_y_latch = value;
            break;
        case ZVB_IO_CONFIG_L0_SCR_X_LOW:
        case ZVB_IO_CONFIG_L1_SCR_X_LOW:
            zvb->ctrl.scroll_x_latch = value;
            break;
        case ZVB_IO_CONFIG_L0_SCR_Y_HIGH:
            zvb->ctrl.l0_scroll_y = (((value & ZVB_SCROLL_Y_HIGH_MASK) << 8) | zvb->ctrl.scroll_y_latch) % ZVB_SCROLL_Y_WRAP;
            break;
        case ZVB_IO_CONFIG_L1_SCR_Y_HIGH:
            zvb->ctrl.l1_scroll_y = (((value & ZVB_SCROLL_Y_HIGH_MASK) << 8) | zvb->ctrl.scroll_y_latch) % ZVB_SCROLL_Y_WRAP;
            break;
        case ZVB_IO_CONFIG_L0_SCR_X_HIGH:
            zvb->ctrl.l0_scroll_x = (((value & ZVB_SCROLL_X_HIGH_MASK) << 8) | zvb->ctrl.scroll_x_latch) % ZVB_SCROLL_X_WRAP;
            break;
        case ZVB_IO_CONFIG_L1_SCR_X_HIGH:
            zvb->ctrl.l1_scroll_x = (((value & ZVB_SCROLL_X_HIGH_MASK) << 8) | zvb->ctrl.scroll_x_latch) % ZVB_SCROLL_X_WRAP;
            break;

        case ZVB_IO_CONFIG_MODE_REG:
            zvb->mode = value & ZVB_REGISTER_MASK;
            zvb_text_mode(&zvb->text, zvb->mode != MODE_TEXT_320);
            break;
        case ZVB_IO_CONFIG_STATUS_REG:
            zvb->status.vid_ena = status.vid_ena;
#if ZVB_BLITTER_SOFTWARE
            if (zvb->status.v_blank) {
                zvb->screen_enabled = status.vid_ena;
            }
#endif
            zvb->status.h_int_ena = status.h_int_ena;
            zvb->status.v_int_ena = status.v_int_ena;
            zvb_update_interrupts(zvb);
            break;
        case ZVB_IO_CONFIG_INT_STATUS_REG:
            zvb->blank_latches &= ~(value & ZVB_BLANK_LATCH_MASK);
            zvb_update_interrupts(zvb);
            break;
        default:
            log_err_printf("[ZVB][CTRL] Unsupported write register %x\n", addr);
            break;
    }
}


static void zvb_peripheral_write(zvb_t* zvb, uint8_t bank, uint32_t subaddr, uint8_t data)
{
    switch (bank) {
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
        case ZVB_IO_MAPPING_RPU:
            zvb_rpu_write(&zvb->rpu, subaddr, data);
            break;
        case ZVB_IO_MAPPING_TIMER:
            zvb_timer_write(&zvb->peri_timer, subaddr, data);
            break;
        default:
            break;
    }
#if ZVB_BLITTER_SOFTWARE
    if (!zvb->clock_syncing) {
        zvb_schedule_sound(zvb);
    }
#endif
    zvb_update_interrupts(zvb);
}

static void zvb_io_write(device_t* dev, uint32_t addr, uint8_t data)
{
    zvb_t* zvb = (zvb_t*) dev;
#if ZVB_BLITTER_SOFTWARE
    zvb_sync_clocks(zvb, vtimer_now_ns());
#endif
    /* Video Board configuration goes from 0x00 to 0x0F included */
    if (addr >= ZVB_IO_SCRAT0_REG && addr <= ZVB_IO_SCRAT3_REG) {
        zvb->scratch[addr - ZVB_IO_SCRAT0_REG] = data;
    } else if (addr == ZVB_IO_BANK_REG) {
        zvb->io_bank = data & ZVB_IO_BANK_MASK;
    } else if (addr == ZVB_MEM_START_REG) {
        /* Zeal uses fixed VRAM at 1 MiB; relocation writes are ignored. */
        return;
    } else if (addr >= ZVB_IO_CONF_START && addr < ZVB_IO_CONF_END) {
        const uint32_t subaddr = addr - ZVB_IO_CONF_START;
        zvb_io_write_control(zvb, subaddr, data);
    } else if (addr >= ZVB_IO_BANK_START && addr < ZVB_IO_BANK_END) {
        const uint32_t subaddr = addr - ZVB_IO_BANK_START;
        zvb_peripheral_write(zvb, zvb->io_bank, subaddr, data);
    }
}


#if CONFIG_ENABLE_DEBUGGER
/**
 * @brief Create the CPU Image and its GPU Texture for one VRAM debug view.
 * The image keeps its pixel buffer, which is filled by the CPU debug renderer
 * and uploaded to the texture with UpdateTexture().
 */
static void zvb_debug_tex_init(zvb_t* dev, dbg_vram_t view, int width, int height)
{
    dev->debug_img[view] = GenImageColor(width, height, BLANK);
    dev->debug_tex[view] = LoadTextureFromImage(dev->debug_img[view]);
}
#endif


static void zvb_fsm_next(void* userdata);


int zvb_init(zvb_t* dev, const zvb_config_t* config, mmu_t* mmu)
{
    if (dev == NULL || config == NULL || mmu == NULL) {
        return 1;
    }

    const bool rendering_enabled = config->rendering_enabled;

    /* Initialize the structure and register it on both the memory and I/O buses */
    memset(dev, 0, sizeof(zvb_t));
    device_init_mem(DEVICE(dev), "zvb_dev", zvb_mem_read, zvb_mem_write, ZVB_MEM_SIZE);
    device_init_io(DEVICE(dev),  "zvb_dev", zvb_io_read, zvb_io_write, ZVB_IO_SIZE);
    device_register_reset(DEVICE(dev), zvb_reset);
    dev->mode = MODE_DEFAULT;
    dev->rendering_enabled = rendering_enabled;
    dev->pio = config->pio;

    zvb_palette_init(&dev->palette, rendering_enabled);
    zvb_font_init(&dev->font, rendering_enabled);
    zvb_tilemap_init(&dev->layers, rendering_enabled);
    zvb_tileset_init(&dev->tileset, rendering_enabled);
    zvb_text_init(&dev->text);
    zvb_sprites_init(&dev->sprites, rendering_enabled);
    zvb_spi_init(&dev->spi);
    zvb_crc32_init(&dev->peri_crc32);
    zvb_sound_init(&dev->sound, rendering_enabled);
    zvb_dma_init(&dev->dma, mmu, !config->dma_disabled);
    zvb_rpu_init(&dev->rpu);
    zvb_timer_init(&dev->peri_timer, zvb_update_interrupts, dev);

    if (dev->rendering_enabled) {
#if CONFIG_ENABLE_DEBUGGER
        zvb_debug_tex_init(dev, DBG_TILEMAP_LAYER0, ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_HEIGHT);
        zvb_debug_tex_init(dev, DBG_TILEMAP_LAYER1, ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_HEIGHT);
        /* Count the grid in the width. For the tileset, use a 16x32 tiles size */
        zvb_debug_tex_init(dev, DBG_TILESET, SIZE_WITH_GRID(16, 16), SIZE_WITH_GRID(16, 32));
        zvb_debug_tex_init(dev, DBG_PALETTE, SIZE_WITH_GRID(16, 16), SIZE_WITH_GRID(16, 16));
        zvb_debug_tex_init(dev, DBG_FONT,    SIZE_WITH_GRID(8, 16),  SIZE_WITH_GRID(12, 16));
#endif
        zvb_blitter_init(dev);
    }

    /* Set the state to STATE_RENDERING, waiting for the next event */
    dev->state = STATE_RENDERING;
    vtimer_init_node(&dev->timer, zvb_fsm_next, dev);
#if ZVB_BLITTER_SOFTWARE
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
    dev->scanline_requested = !config->scanline_disabled;
    dev->scanline_rendering = dev->scanline_requested;
#endif
    vtimer_init_node(&dev->sound_event, zvb_sound_next, dev);
    dev->clock_ns = vtimer_now_ns();
    dev->line_start_ns = dev->clock_ns;
    vtimer_schedule_ns(&dev->timer, ZVB_VISIBLE_LINE_NS);
#else
    vtimer_schedule_tstates(&dev->timer, s_tstates_remaining[dev->state]);
#endif

    /* Enable the screen by default */
    dev->status.vid_ena = 1;
    zvb_update_interrupts(dev);
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
    zvb_rpu_reset(&zvb->rpu);
    zvb_timer_reset(&zvb->peri_timer);
    zvb->mode = MODE_DEFAULT;
    memset(&zvb->ctrl, 0, sizeof(zvb->ctrl));
    zvb->status.raw = ZVB_RESET_STATUS;
    zvb->screen_enabled = false;
    zvb->io_bank = 0;
    zvb->blank_latches = 0;
    vtimer_cancel(&zvb->timer);
    zvb->current_hpos = 0;
    zvb->current_scanline = 0;
    zvb->state = STATE_RENDERING;
    zvb->need_render = false;
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
    zvb->scanline_rendering = zvb->scanline_requested;
    zvb->blitter.raster_frame = false;
#endif
#if ZVB_BLITTER_SOFTWARE
    vtimer_cancel(&zvb->sound_event);
    zvb->clock_ns = vtimer_now_ns();
    zvb->line_start_ns = zvb->clock_ns;
    zvb->clock_syncing = false;
    vtimer_schedule_ns(&zvb->timer, ZVB_VISIBLE_LINE_NS);
#else
    vtimer_schedule_tstates(&zvb->timer, s_tstates_remaining[STATE_RENDERING]);
#endif
    zvb_update_interrupts(zvb);
}


/**
 * @brief Present black when the VGA output is disabled.
 */
static void zvb_render_disabled_mode(zvb_t* zvb)
{
    BeginTextureMode(zvb->blitter.main_texture);
        ClearBackground(BLACK);
    EndTextureMode();
}


/* Prepare the rendering by updating the underneath textures */
bool zvb_prepare_render(zvb_t* zvb)
{
    /* In headless mode, no need to update any texture */
    if (!zvb->rendering_enabled) {
        return false;
    }

    /* Only update the texture if we are going to render anything */
    if (!zvb->need_render) {
        return false;
    }

    switch (zvb->mode) {
        case MODE_TEXT_640:
        case MODE_TEXT_320:
            zvb_blitter_prepare_render_text_mode(zvb);
            break;

        case MODE_BITMAP_256:
        case MODE_BITMAP_320:
            zvb_blitter_prepare_render_bitmap_mode(zvb);
            break;

        default:
            zvb_blitter_prepare_render_gfx_mode(zvb);
            break;
    }

    return true;
}


void zvb_render(zvb_t* zvb)
{
    if (!zvb->rendering_enabled) {
        zvb->need_render = false;
        return;
    }

    if (zvb->need_render == false) {
        return;
    }

    zvb->need_render = false;

#if CONFIG_PROFILE_RENDER
    const double profile_start = GetTime();
#endif

#if ZVB_BLITTER_SOFTWARE
    bool frame_ready = zvb->screen_enabled;
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
    frame_ready |= zvb->blitter.raster_frame;
#endif
#else
    const bool frame_ready = zvb->status.vid_ena;
#endif
    if (frame_ready) {
        switch (zvb->mode) {
            case MODE_TEXT_640:
            case MODE_TEXT_320:
                zvb_blitter_render_text_mode(zvb);
                break;

            case MODE_BITMAP_256:
            case MODE_BITMAP_320:
                zvb_blitter_render_bitmap_mode(zvb);
                break;

            default:
                zvb_blitter_render_gfx_mode(zvb);
                break;
        }
    } else {
        zvb_render_disabled_mode(zvb);
    }

#if CONFIG_PROFILE_RENDER
    const double elapsed = GetTime() - profile_start;
    s_render_profile.zvb_total += elapsed;
    if (elapsed > s_render_profile.zvb_max) {
        s_render_profile.zvb_max = elapsed;
    }
    s_render_profile.zvb_calls++;
#endif
}

#if CONFIG_PROFILE_RENDER
void zvb_profile_frame(double elapsed_seconds)
{
    if (!config.arguments.profile) {
        return;
    }

    const double now = GetTime();
    if (s_render_profile.window_start == 0.0) {
        s_render_profile.window_start = now;
    }
    s_render_profile.frame_total += elapsed_seconds;
    if (elapsed_seconds > s_render_profile.frame_max) {
        s_render_profile.frame_max = elapsed_seconds;
    }
    s_render_profile.frames++;

    if (now - s_render_profile.window_start < 1.0) {
        return;
    }

    const double frame_avg = s_render_profile.frames > 0
        ? s_render_profile.frame_total / s_render_profile.frames : 0.0;
    const double zvb_avg = s_render_profile.zvb_calls > 0
        ? s_render_profile.zvb_total / s_render_profile.zvb_calls : 0.0;
    log_printf(
        "[PROFILE][ZVB] frames=%llu frame=%.2f/%.2fms zvb=%.2f/%.2fms\n",
        (unsigned long long)s_render_profile.frames,
        frame_avg * 1000.0,
        s_render_profile.frame_max * 1000.0,
        zvb_avg * 1000.0,
        s_render_profile.zvb_max * 1000.0);

    memset(&s_render_profile, 0, sizeof(s_render_profile));
    s_render_profile.window_start = now;
}
#endif


void zvb_force_render(zvb_t* zvb)
{
    if (!zvb->rendering_enabled) {
        return;
    }
    zvb->need_render = true;
    zvb_prepare_render(zvb);
    zvb_render(zvb);
}


#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
void zvb_set_scanline_rendering(zvb_t* zvb, bool enabled)
{
    zvb->scanline_requested = enabled;
}

bool zvb_scanline_rendering_requested(const zvb_t* zvb)
{
    return zvb->scanline_requested;
}
#endif

#if ZVB_BLITTER_SOFTWARE
static void zvb_rpu_load(void* userdata, uint16_t address, uint8_t data)
{
    zvb_t* zvb = userdata;
    zvb_mem_write(DEVICE(zvb), address, data);
}

static void zvb_schedule_sound(zvb_t* zvb)
{
    vtimer_cancel(&zvb->sound_event);
    const uint32_t clocks = zvb_sound_clocks_until_interrupt(&zvb->sound);
    if (clocks != 0) {
        vtimer_schedule_at_ns(&zvb->sound_event,
                             zvb->clock_ns + clocks * ZVB_MASTER_CLOCK_NS);
    }
}

static void zvb_sound_next(void* userdata)
{
    zvb_t* zvb = userdata;
    zvb_sync_clocks(zvb, zvb->sound_event.deadline);
}

/* Advance clocked peripherals between observable bus accesses and line events.
 * The next line deadline bounds execution, including looping RPU programs. */
static void zvb_sync_clocks(zvb_t* zvb, uint64_t target_ns)
{
    if (zvb->clock_syncing) {
        return;
    }
    if (target_ns > zvb->timer.deadline) {
        target_ns = zvb->timer.deadline;
    }
    zvb->clock_syncing = true;
    while (zvb->clock_ns + ZVB_MASTER_CLOCK_NS <= target_ns) {
        const uint16_t hpos = (zvb->clock_ns - zvb->line_start_ns) / ZVB_PIXEL_NS;
        if (zvb_rpu_active(&zvb->rpu)) {
            zvb_rpu_clock(&zvb->rpu, hpos, zvb->current_scanline, zvb_rpu_load, zvb);
        }
        zvb_sound_clock(&zvb->sound);
        if (zvb->spi.busy) {
            zvb_spi_clock(&zvb->spi);
        }
        zvb->clock_ns += ZVB_MASTER_CLOCK_NS;
    }
    zvb->clock_syncing = false;
    zvb_schedule_sound(zvb);
    zvb_update_interrupts(zvb);
}

static void zvb_software_raster_next(zvb_t* zvb)
{
    const uint64_t deadline = zvb->timer.deadline;
    zvb_sync_clocks(zvb, deadline);
    if (zvb->state == STATE_RENDERING) {
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
        if (zvb->rendering_enabled && zvb->current_scanline < ZVB_MAX_RES_HEIGHT) {
            zvb_blitter_render_scanline(zvb, zvb->current_scanline);
        }
#endif
        zvb->current_hpos = ZVB_MAX_RES_WIDTH;
        zvb->state = STATE_HBLANK;
        zvb->status.h_blank = 1;
        zvb->blank_latches |= ZVB_HBLANK_LATCH;
        vtimer_schedule_at_ns(&zvb->timer, deadline + ZVB_HBLANK_NS);
    } else {
        zvb->current_hpos = 0;
        zvb->current_scanline++;
        zvb->line_start_ns = deadline;
        zvb->state = STATE_RENDERING;
        zvb->status.h_blank = 0;
        if (zvb->current_scanline == ZVB_MAX_RES_HEIGHT) {
            zvb->need_render = true;
            zvb->status.v_blank = 1;
            zvb->blank_latches |= ZVB_VBLANK_LATCH;
            zvb_text_info_t info;
            zvb_text_update(&zvb->text, &info);
        } else if (zvb->current_scanline >= ZVB_TOTAL_SCANLINES) {
            zvb->current_scanline = 0;
            zvb->status.v_blank = 0;
#if ZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING
            if (zvb->scanline_rendering != zvb->scanline_requested) {
                zvb->scanline_rendering = zvb->scanline_requested;
                zvb->blitter.raster_frame = false;
            }
#endif
        }
        vtimer_schedule_at_ns(&zvb->timer, deadline + ZVB_VISIBLE_LINE_NS);
    }
    /* Video enable is sampled in vertical blank and held for the visible frame;
     * reading the requested vid_ena directly would permit mid-frame changes. */
    if (zvb->status.v_blank) {
        zvb->screen_enabled = zvb->status.vid_ena;
    }
    zvb_update_interrupts(zvb);
}
#endif

static void zvb_fsm_next(void* userdata)
{
    zvb_t* zvb = (zvb_t*) userdata;

#if ZVB_BLITTER_SOFTWARE
    zvb_software_raster_next(zvb);
#else
    if (zvb->state == STATE_RENDERING) {
        zvb->state = STATE_HBLANK;
        zvb->status.h_blank = 1;
        /* Ignore v-blank scanlines */
        if (zvb->current_scanline < 480 && zvb->rendering_enabled) {
            zvb_blitter_render_scanline(zvb, zvb->current_scanline);
        }
        vtimer_schedule_tstates(&zvb->timer, s_tstates_remaining[STATE_HBLANK]);
    } else { /* STATE_HBLANK */
        zvb->state = STATE_RENDERING;
        zvb->status.h_blank = 0;
        zvb->current_scanline++;
        /* If we have reached line 480, we enter V-Blank state */
        if (zvb->current_scanline == 480) {
            zvb->need_render = true;
            zvb->status.v_blank = 1;
        } else if (zvb->current_scanline >= 524) {
            /* End of v-blank, we start from the top again */
            zvb->current_scanline = 0;
            zvb->status.v_blank = 0;
        }
        vtimer_schedule_tstates(&zvb->timer, s_tstates_remaining[STATE_RENDERING]);
    }
#endif
}


void zvb_deinit(zvb_t* zvb)
{
    vtimer_cancel(&zvb->timer);
    vtimer_cancel(&zvb->peri_timer.event);
#if ZVB_BLITTER_SOFTWARE
    vtimer_cancel(&zvb->sound_event);
#endif
    zvb_sound_deinit(&zvb->sound);
    if (!zvb->rendering_enabled) {
        return;
    }

    zvb_blitter_deinit(zvb);

#if CONFIG_ENABLE_DEBUGGER
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        UnloadTexture(zvb->debug_tex[i]);
        UnloadImage(zvb->debug_img[i]);
    }
#endif
}
