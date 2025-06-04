#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "raylib.h"
#include "utils/helpers.h"
#include "utils/paths.h"
#include "hw/zvb/zvb.h"
#include "hw/zvb/default_font.h"


#define BENCHMARK           0


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


/**
 * @brief Names of the variables in the shader
 */
#define SHADER_VIDMODE_NAME         "video_mode"
#define SHADER_PALETTE_NAME         "palette"
#define SHADER_FONT_NAME            "font"
#define SHADER_TILEMAPS_NAME        "tilemaps"
#define SHADER_TILESET_NAME         "tileset"
#define SHADER_CURPOS_NAME          "curpos"
#define SHADER_CURCOLOR_NAME        "curcolor"
#define SHADER_CURCHAR_NAME         "curchar"
#define SHADER_TSCROLL_NAME         "scroll"
#define SHADER_SPRITES_NAME         "sprites"
/* Scrolling vlaues for GFX mode */
#define SHADER_SCROLL0_NAME         "scroll_l0"
#define SHADER_SCROLL1_NAME         "scroll_l1"


static const long s_tstates_remaining[STATE_COUNT] = {
    /* The raster spends 15.253 ms in the visible area */
    [STATE_IDLE]         = US_TO_TSTATES(15253),
    /* The raster stays in V-Blank during 1.430ms  */
    [STATE_VBLANK]       = US_TO_TSTATES(1430),
};


static uint8_t zvb_mem_read(device_t* dev, uint32_t addr)
{
    (void) dev;
    (void) addr;
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
            printf("[ZVB][CTRL] Unknwon register %x\n", addr);
            break;
    }

    return 0;
}


static uint8_t zvb_io_read(device_t* dev, uint32_t addr)
{
    zvb_t* zvb = (zvb_t*) dev;

    /* Video Board configuraiton goes from 0x00 to 0x0F included */
    if (addr == ZVB_IO_BANK_REG) {
    } else if (addr == ZVB_MEM_START_REG) {
    } else if (addr >= ZVB_IO_CONF_START && addr < ZVB_IO_CONF_END) {
        const uint32_t subaddr = addr - ZVB_IO_CONF_START;
        return zvb_io_read_control(zvb, subaddr);
    } else if (addr >= ZVB_IO_BANK_START && addr < ZVB_IO_BANK_END) {
        const uint32_t subaddr = addr - ZVB_IO_BANK_START;
        if (zvb->io_bank == ZVB_IO_MAPPING_TEXT) {
            return zvb_text_read(&zvb->text, subaddr);
        } else if (zvb->io_bank == ZVB_IO_MAPPING_SPI) {
            return zvb_spi_read(&zvb->spi, subaddr);
        } else if (zvb->io_bank == ZVB_IO_MAPPING_CRC) {
            return zvb_crc32_read(&zvb->peri_crc32, subaddr);
        } else if (zvb->io_bank == ZVB_IO_MAPPING_SOUND) {
            // peri_sound.io_write(subaddr, data);
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
            printf("[ZVB][CTRL] Unknwon register %x\n", addr);
            break;
    }
}


static void zvb_io_write(device_t* dev, uint32_t addr, uint8_t data)
{
    zvb_t* zvb = (zvb_t*) dev;
    /* Video Board configuration goes from 0x00 to 0x0F included */
    if (addr == ZVB_IO_BANK_REG) {
        zvb->io_bank = data;
    } else if (addr == ZVB_MEM_START_REG) {
        printf("[WARNING] zvb memory mapping register is not supported\n");
    } else if (addr >= ZVB_IO_CONF_START && addr < ZVB_IO_CONF_END) {
        const uint32_t subaddr = addr - ZVB_IO_CONF_START;
        zvb_io_write_control(zvb, subaddr, data);
    } else if (addr >= ZVB_IO_BANK_START && addr < ZVB_IO_BANK_END) {
        const uint32_t subaddr = addr - ZVB_IO_BANK_START;
        if (zvb->io_bank == ZVB_IO_MAPPING_TEXT) {
            zvb_text_write(&zvb->text, subaddr, data, &zvb->layers);
        } else if (zvb->io_bank == ZVB_IO_MAPPING_SPI) {
            zvb_spi_write(&zvb->spi, subaddr, data);
        } else if (zvb->io_bank == ZVB_IO_MAPPING_CRC) {
            zvb_crc32_write(&zvb->peri_crc32, subaddr, data);
        } else if (zvb->io_bank == ZVB_IO_MAPPING_SOUND) {
            // peri_sound.io_write(subaddr, data);
        }
    }
}


int zvb_init(zvb_t* dev, bool flipped_y)
{
    if (dev == NULL) {
        return 1;
    }

    /* Initialize the structure and register it on both the memory and I/O buses */
    memset(dev, 0, sizeof(zvb_t));
    device_init_mem(DEVICE(dev), "zvb_dev", zvb_mem_read, zvb_mem_write, ZVB_MEM_SIZE);
    device_init_io(DEVICE(dev),  "zvb_dev", zvb_io_read, zvb_io_write, ZVB_IO_SIZE);
    dev->mode = MODE_DEFAULT;

    /* Initialize the sub-components */
    char text_shader_path[PATH_MAX];
    char gfx_shader_path[PATH_MAX];

    // TODO: decide on ES3 or GL 3.3 (ie; frag or glsl)
    // (void) get_install_dir_file(text_shader_path, "hw/zvb/text_shader.glsl");
    // (void) get_install_dir_file(gfx_shader_path, "hw/zvb/gfx_shader.glsl");
    (void) get_install_dir_file(text_shader_path, "hw/zvb/text_shader.frag");
    (void) get_install_dir_file(gfx_shader_path, "hw/zvb/gfx_shader.frag");

    
    dev->text_shader = LoadShader(NULL, text_shader_path);
    dev->gfx_shader  = LoadShader(NULL, gfx_shader_path);
    zvb_palette_init(&dev->palette);
    zvb_font_init(&dev->font);
    zvb_tilemap_init(&dev->layers);
    zvb_tileset_init(&dev->tileset);
    zvb_text_init(&dev->text);
    zvb_sprites_init(&dev->sprites);
    zvb_spi_init(&dev->spi);
    zvb_crc32_init(&dev->peri_crc32);

    dev->tex_dummy = LoadRenderTexture(ZVB_MAX_RES_WIDTH, ZVB_MAX_RES_HEIGHT);

    /* Get the indexes of the objects in the shaders */
    const Shader text_shader = dev->text_shader;
    dev->text_shader_idx[TEXT_SHADER_VIDMODE_IDX]  = GetShaderLocation(text_shader, SHADER_VIDMODE_NAME);
    dev->text_shader_idx[TEXT_SHADER_TILEMAPS_IDX] = GetShaderLocation(text_shader, SHADER_TILEMAPS_NAME);
    dev->text_shader_idx[TEXT_SHADER_FONT_IDX]     = GetShaderLocation(text_shader, SHADER_FONT_NAME);
    dev->text_shader_idx[TEXT_SHADER_PALETTE_IDX]  = GetShaderLocation(text_shader, SHADER_PALETTE_NAME);
    dev->text_shader_idx[TEXT_SHADER_CURPOS_IDX]   = GetShaderLocation(text_shader, SHADER_CURPOS_NAME);
    dev->text_shader_idx[TEXT_SHADER_CURCOLOR_IDX] = GetShaderLocation(text_shader, SHADER_CURCOLOR_NAME);
    dev->text_shader_idx[TEXT_SHADER_CURCHAR_IDX]  = GetShaderLocation(text_shader, SHADER_CURCHAR_NAME);
    dev->text_shader_idx[TEXT_SHADER_TSCROLL_IDX]  = GetShaderLocation(text_shader, SHADER_TSCROLL_NAME);

    const Shader gfx_shader = dev->gfx_shader;
    dev->gfx_shader_idx[GFX_SHADER_VIDMODE_IDX]  = GetShaderLocation(gfx_shader, SHADER_VIDMODE_NAME);
    dev->gfx_shader_idx[GFX_SHADER_TILEMAPS_IDX] = GetShaderLocation(gfx_shader, SHADER_TILEMAPS_NAME);
    dev->gfx_shader_idx[GFX_SHADER_TILESET_IDX]  = GetShaderLocation(gfx_shader, SHADER_TILESET_NAME);
    dev->gfx_shader_idx[GFX_SHADER_SPRITES_IDX]  = GetShaderLocation(gfx_shader, SHADER_SPRITES_NAME);
    dev->gfx_shader_idx[GFX_SHADER_SCROLL0_IDX]  = GetShaderLocation(gfx_shader, SHADER_SCROLL0_NAME);
    dev->gfx_shader_idx[GFX_SHADER_SCROLL1_IDX]  = GetShaderLocation(gfx_shader, SHADER_SCROLL1_NAME);
    dev->gfx_shader_idx[GFX_SHADER_PALETTE_IDX]  = GetShaderLocation(gfx_shader, SHADER_PALETTE_NAME);


    /* Set the state to STATE_IDLE, waiting for the next event */
    dev->state = STATE_IDLE;
    dev->tstates_counter = s_tstates_remaining[dev->state];

    /* Enable the screen by default */
    dev->status.vid_ena = 1;
    dev->need_render = false;

    /* For the debugger */
    dev->flipped_y = flipped_y;

    return 0;
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
    zvb_font_update(&zvb->font);
    zvb_tilemap_update(&zvb->layers);
}


static void zvb_render_text_mode(zvb_t* zvb)
{
    /* Update the palette to flush the changes to the shader */
    const Shader shader = zvb->text_shader;
    const int mode_idx         = zvb->text_shader_idx[TEXT_SHADER_VIDMODE_IDX];
    const int tilemaps_idx     = zvb->text_shader_idx[TEXT_SHADER_TILEMAPS_IDX];
    const int font_idx         = zvb->text_shader_idx[TEXT_SHADER_FONT_IDX];
    const int cursor_pos_idx   = zvb->text_shader_idx[TEXT_SHADER_CURPOS_IDX];
    const int cursor_color_idx = zvb->text_shader_idx[TEXT_SHADER_CURCOLOR_IDX];
    const int cursor_char_idx  = zvb->text_shader_idx[TEXT_SHADER_CURCHAR_IDX];
    const int scroll_idx       = zvb->text_shader_idx[TEXT_SHADER_TSCROLL_IDX];
    const int palette_idx      = zvb->text_shader_idx[TEXT_SHADER_PALETTE_IDX];

    /* Get the cursor position and its color */
    zvb_text_info_t info;
    zvb_text_update(&zvb->text, &info);

    BeginShaderMode(shader);
        zvb_palette_update(&zvb->palette, &zvb->text_shader, palette_idx);
        /* Transfer all the texture to the GPU */
        SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
        SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
        SetShaderValueTexture(shader, font_idx, zvb_font_texture(&zvb->font));
        /* Transfer the text-related variables */
        SetShaderValue(shader, cursor_pos_idx,   &info.pos, SHADER_UNIFORM_IVEC2);
        SetShaderValue(shader, cursor_color_idx, &info.color, SHADER_UNIFORM_IVEC2);
        SetShaderValue(shader, cursor_char_idx,  &info.charidx, SHADER_UNIFORM_INT);
        SetShaderValue(shader, scroll_idx,  &info.scroll, SHADER_UNIFORM_IVEC2);

        /* Flip the screen in Y since OpenGL treats (0,0) as the bottom left pixel of the screen */
        DrawTextureRec(zvb->tex_dummy.texture,
                        (Rectangle){ 0, 0,
                                     ZVB_MAX_RES_WIDTH,
                                     zvb->flipped_y ? -ZVB_MAX_RES_HEIGHT : ZVB_MAX_RES_HEIGHT },
                        (Vector2){ 0, 0 },
                        WHITE);
    EndShaderMode();
}


static void zvb_prepare_render_gfx_mode(zvb_t* zvb)
{
    zvb_tileset_update(&zvb->tileset);
    zvb_tilemap_update(&zvb->layers);
    zvb_sprites_update(&zvb->sprites);
}

/**
 * @brief Render the screen in graphics mode
 */
static void zvb_render_gfx_mode(zvb_t* zvb)
{
    const Shader shader = zvb->gfx_shader;

    const int mode_idx     = zvb->gfx_shader_idx[GFX_SHADER_VIDMODE_IDX];
    const int tilemaps_idx = zvb->gfx_shader_idx[GFX_SHADER_TILEMAPS_IDX];
    const int tileset_idx  = zvb->gfx_shader_idx[GFX_SHADER_TILESET_IDX];
    const int sprites_idx  = zvb->gfx_shader_idx[GFX_SHADER_SPRITES_IDX];
    const int scroll0_idx  = zvb->gfx_shader_idx[GFX_SHADER_SCROLL0_IDX];
    const int scroll1_idx  = zvb->gfx_shader_idx[GFX_SHADER_SCROLL1_IDX];
    const int palette_idx  = zvb->gfx_shader_idx[GFX_SHADER_PALETTE_IDX];

    BeginShaderMode(shader);
        zvb_palette_update(&zvb->palette, &zvb->gfx_shader, palette_idx);
        /* Transfer all the texture to the GPU */
        SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
        SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
        SetShaderValueTexture(shader, tileset_idx, *zvb_tileset_texture(&zvb->tileset));
        SetShaderValueTexture(shader, sprites_idx, *zvb_sprites_texture(&zvb->sprites));
        /* Transfer the text-related variables */
        SetShaderValue(shader, scroll0_idx,  &zvb->ctrl.l0_scroll_x, SHADER_UNIFORM_IVEC2);
        SetShaderValue(shader, scroll1_idx,  &zvb->ctrl.l1_scroll_x, SHADER_UNIFORM_IVEC2);

        /* Flip the screen in Y since OpenGL treats (0,0) as the bottom left pixel of the screen */
        DrawTextureRec(zvb->tex_dummy.texture,
                        (Rectangle){ 0, 0,
                                     ZVB_MAX_RES_WIDTH,
                                     zvb->flipped_y ? -ZVB_MAX_RES_HEIGHT : ZVB_MAX_RES_HEIGHT },
                        (Vector2){ 0, 0 },
                        WHITE);
    EndShaderMode();
}


/* Prepare the rendering by updating the udnerneath textures */
bool zvb_prepare_render(zvb_t* zvb)
{
    /* Only update the texture if we are going to render anything */
    if (!zvb->need_render || !zvb->status.vid_ena) {
        return false;
    }

    if (zvb->mode == MODE_TEXT_640 || zvb->mode == MODE_TEXT_320) {
        zvb_prepare_render_text_mode(zvb);
    } else {
        zvb_prepare_render_gfx_mode(zvb);
    }

    return true;
}


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
        if (zvb->mode == MODE_TEXT_640 || zvb->mode == MODE_TEXT_320) {
            zvb_render_text_mode(zvb);
        } else {
            zvb_render_gfx_mode(zvb);
        }
    } else {
        zvb_render_disabled_mode(zvb);
    }

#if BENCHMARK
    double endTime = GetTime();
    double elapsedTime = endTime - startTime;
    average += elapsedTime;
    if (++counter == 60) {
        printf("Time taken : %f ms\n", (average / 60) * 1000);
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
}
