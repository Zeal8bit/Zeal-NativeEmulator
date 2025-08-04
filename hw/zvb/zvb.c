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
 * @brief Calculate the size (width or height) counting a grid
 */
#define SIZE_WITH_GRID(TILESIZE, TILECOUNT)    ((((TILESIZE)+1)*TILECOUNT)+1)

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
        return 0;
    } else if (addr == ZVB_IO_MINOR_REG) {
        return 2;
    } else if (addr == ZVB_IO_MAJOR_REG) {
        return 0;
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


static Shader zvb_load_shader(const char* path)
{
    const char *header =
#if OPENGL_ES
    "#version 300 es\n"
    "#define OPENGL_ES 1\n";
#else
    "#version 330 core\n";
#endif
    log_printf("Compiling shader %s\n", path);

    char *source = LoadFileText(path);
    if (!source) {
        log_err_printf("Failed to load shader file: %s", path);
        return (Shader){0};
    }

    size_t headerlen = strlen(header);
    size_t sourcelen = strlen(source);
    char *fullSource = malloc(headerlen + sourcelen + 1);
    if (!fullSource) {
        log_err_printf("Memory allocation failed for shader source");
        UnloadFileText(source);
        return (Shader){0};
    }


    memcpy(fullSource, header, headerlen);
    /* Include NULL terminator */
    memcpy(fullSource + headerlen, source, sourcelen + 1);

    UnloadFileText(source);

    Shader shader = LoadShaderFromMemory(NULL, fullSource);

    free(fullSource);
    return shader;
}


static void zvb_shader_init(zvb_t* dev)
{
    char path[PATH_MAX];

    /* Get the indexes of the objects in the shaders */
    zvb_shader_t* st_shader = &dev->shaders[SHADER_TEXT];
    Shader shader = zvb_load_shader(get_shaders_path(path, "text_shader.glsl"));
    st_shader->shader = shader;
    st_shader->objects[TEXT_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[TEXT_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[TEXT_SHADER_FONT_IDX]     = GetShaderLocation(shader, SHADER_FONT_NAME);
    st_shader->objects[TEXT_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);
    st_shader->objects[TEXT_SHADER_CURPOS_IDX]   = GetShaderLocation(shader, SHADER_CURPOS_NAME);
    st_shader->objects[TEXT_SHADER_CURCOLOR_IDX] = GetShaderLocation(shader, SHADER_CURCOLOR_NAME);
    st_shader->objects[TEXT_SHADER_CURCHAR_IDX]  = GetShaderLocation(shader, SHADER_CURCHAR_NAME);
    st_shader->objects[TEXT_SHADER_TSCROLL_IDX]  = GetShaderLocation(shader, SHADER_TSCROLL_NAME);

    /* Text debug shaders */
    st_shader = &dev->shaders[SHADER_TEXT_DEBUG];
    shader = zvb_load_shader(get_shaders_path(path, "text_debug.glsl"));
    st_shader->shader = shader;
    st_shader->objects[TEXT_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[TEXT_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[TEXT_SHADER_FONT_IDX]     = GetShaderLocation(shader, SHADER_FONT_NAME);
    st_shader->objects[TEXT_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);
    st_shader->objects[TEXT_SHADER_DBGMODE_IDX]  = GetShaderLocation(shader, "debug_mode");

    st_shader = &dev->shaders[SHADER_GFX];
    shader = zvb_load_shader(get_shaders_path(path, "gfx_shader.glsl"));
    st_shader->shader = shader;
    st_shader->objects[GFX_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[GFX_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[GFX_SHADER_TILESET_IDX]  = GetShaderLocation(shader, SHADER_TILESET_NAME);
    st_shader->objects[GFX_SHADER_SPRITES_IDX]  = GetShaderLocation(shader, SHADER_SPRITES_NAME);
    st_shader->objects[GFX_SHADER_SCROLL0_IDX]  = GetShaderLocation(shader, SHADER_SCROLL0_NAME);
    st_shader->objects[GFX_SHADER_SCROLL1_IDX]  = GetShaderLocation(shader, SHADER_SCROLL1_NAME);
    st_shader->objects[GFX_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);

    st_shader = &dev->shaders[SHADER_BITMAP];
    shader = zvb_load_shader(get_shaders_path(path, "bitmap_shader.glsl"));
    st_shader->shader = shader;
    st_shader->objects[GFX_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[GFX_SHADER_TILESET_IDX]  = GetShaderLocation(shader, SHADER_TILESET_NAME);
    st_shader->objects[GFX_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);

    st_shader = &dev->shaders[SHADER_GFX_DEBUG];
    shader = zvb_load_shader(get_shaders_path(path, "gfx_debug.glsl"));
    st_shader->shader = shader;
    st_shader->objects[GFX_SHADER_VIDMODE_IDX]  = GetShaderLocation(shader, SHADER_VIDMODE_NAME);
    st_shader->objects[GFX_SHADER_TILEMAPS_IDX] = GetShaderLocation(shader, SHADER_TILEMAPS_NAME);
    st_shader->objects[GFX_SHADER_TILESET_IDX]  = GetShaderLocation(shader, SHADER_TILESET_NAME);
    st_shader->objects[GFX_SHADER_PALETTE_IDX]  = GetShaderLocation(shader, SHADER_PALETTE_NAME);
    st_shader->objects[GFX_SHADER_DBGMODE_IDX]  = GetShaderLocation(shader, "debug_mode");
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
    dev->debug_tex[DBG_TILEMAP_LAYER0]  = LoadRenderTexture(ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_HEIGHT);
    dev->debug_tex[DBG_TILEMAP_LAYER1]  = LoadRenderTexture(ZVB_DBG_RES_WIDTH, ZVB_DBG_RES_HEIGHT);
    /* Count the grid in the width. For the tileset, use a 16x32 tiles size */
    dev->debug_tex[DBG_TILESET] = LoadRenderTexture(SIZE_WITH_GRID(16, 16), SIZE_WITH_GRID(16, 32));
    dev->debug_tex[DBG_PALETTE] = LoadRenderTexture(SIZE_WITH_GRID(16, 16), SIZE_WITH_GRID(16, 16));
    dev->debug_tex[DBG_FONT]    = LoadRenderTexture(SIZE_WITH_GRID(8, 16),  SIZE_WITH_GRID(12, 16));

    zvb_shader_init(dev);

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
    zvb_font_update(&zvb->font);
    zvb_tilemap_update(&zvb->layers);
}


static void zvb_render_text_mode(zvb_t* zvb)
{
    /* Update the palette to flush the changes to the shader */
    zvb_shader_t* st_shader = &zvb->shaders[SHADER_TEXT];
    const Shader shader = st_shader->shader;
    const int mode_idx         = st_shader->objects[TEXT_SHADER_VIDMODE_IDX];
    const int tilemaps_idx     = st_shader->objects[TEXT_SHADER_TILEMAPS_IDX];
    const int font_idx         = st_shader->objects[TEXT_SHADER_FONT_IDX];
    const int cursor_pos_idx   = st_shader->objects[TEXT_SHADER_CURPOS_IDX];
    const int cursor_color_idx = st_shader->objects[TEXT_SHADER_CURCOLOR_IDX];
    const int cursor_char_idx  = st_shader->objects[TEXT_SHADER_CURCHAR_IDX];
    const int scroll_idx       = st_shader->objects[TEXT_SHADER_TSCROLL_IDX];
    const int palette_idx      = st_shader->objects[TEXT_SHADER_PALETTE_IDX];

    /* Get the cursor position and its color */
    zvb_text_info_t info;
    zvb_text_update(&zvb->text, &info);

    BeginShaderMode(shader);
        zvb_palette_update(&zvb->palette, &st_shader->shader, palette_idx);
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

/**
 * @brief Render the debug textures when we are in text mode.
 * The `zvb_render` function must be called first.
 */
static void zvb_render_debug_text_mode(zvb_t* zvb)
{
    /* Since we want to generate a debug texture, we only need to set it to debug mode */
    zvb_shader_t* st_shader = &zvb->shaders[SHADER_TEXT_DEBUG];
    const Shader shader = st_shader->shader;
    const int mode_idx     = st_shader->objects[TEXT_SHADER_VIDMODE_IDX];
    const int dbg_mode_idx = st_shader->objects[TEXT_SHADER_DBGMODE_IDX];
    const int tilemaps_idx = st_shader->objects[TEXT_SHADER_TILEMAPS_IDX];
    const int font_idx     = st_shader->objects[TEXT_SHADER_FONT_IDX];
    const int palette_idx  = st_shader->objects[TEXT_SHADER_PALETTE_IDX];
    /* Include the debug grid in the final texture width. Add one pixel to show a red outline */
    const int grid_thickness = 1;
    const int width = TEXT_MAXIMUM_COLUMNS * (TEXT_CHAR_WIDTH + grid_thickness) + 1;
    const int height = TEXT_MAXIMUM_LINES * (TEXT_CHAR_HEIGHT + grid_thickness) + 1;

    /* Tilemap mode */
    int dbg_mode = TEXT_DEBUG_LAYER0_MODE;
    BeginTextureMode(zvb->debug_tex[DBG_TILEMAP_LAYER0]);
        BeginShaderMode(shader);
            ClearBackground(BLANK);
            /* Transfer all the texture to the GPU */
            zvb_palette_force_update(&zvb->palette, &st_shader->shader, palette_idx);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
            SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
            SetShaderValueTexture(shader, font_idx, zvb_font_texture(&zvb->font));
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, width, height },
                            /* Since the texture is bigger than the content, render the content at the top left */
                            (Vector2){ 0, ZVB_DBG_RES_HEIGHT-height },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Font mode, no need to reload all the textures, they are all in the shaders already */
    dbg_mode = TEXT_DEBUG_LAYER1_MODE;
    BeginTextureMode(zvb->debug_tex[DBG_TILEMAP_LAYER1]);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, width, height },
                            /* Since the texture is bigger than the content, render the content at the top left */
                            (Vector2){ 0, ZVB_DBG_RES_HEIGHT-height },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    dbg_mode = TEXT_DEBUG_FONT_MODE;
    RenderTexture* texture = &zvb->debug_tex[DBG_FONT];
    BeginTextureMode(*texture);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();
}

static void zvb_prepare_render_gfx_mode(zvb_t* zvb)
{
    zvb_tileset_update(&zvb->tileset);
    zvb_tilemap_update(&zvb->layers);
    zvb_sprites_update(&zvb->sprites);
}

static void zvb_prepare_render_bitmap_mode(zvb_t* zvb)
{
    zvb_tileset_update(&zvb->tileset);
}

/**
 * @brief Render the screen in bitmap mode
 */

static void zvb_render_bitmap_mode(zvb_t* zvb)
{
    zvb_shader_t* st_shader = &zvb->shaders[SHADER_BITMAP];
    const Shader shader = st_shader->shader;
    const int mode_idx     = st_shader->objects[GFX_SHADER_VIDMODE_IDX];
    const int tileset_idx  = st_shader->objects[GFX_SHADER_TILESET_IDX];
    const int palette_idx  = st_shader->objects[GFX_SHADER_PALETTE_IDX];

    BeginShaderMode(shader);
        zvb_palette_update(&zvb->palette, &st_shader->shader, palette_idx);
        /* Transfer all the texture to the GPU */
        SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
        SetShaderValueTexture(shader, tileset_idx, *zvb_tileset_texture(&zvb->tileset));

        /* Flip the screen in Y since OpenGL treats (0,0) as the bottom left pixel of the screen */
        DrawTextureRec(zvb->tex_dummy.texture,
                        (Rectangle){ 0, 0,
                                     ZVB_MAX_RES_WIDTH,
                                     zvb->flipped_y ? -ZVB_MAX_RES_HEIGHT : ZVB_MAX_RES_HEIGHT },
                        (Vector2){ 0, 0 },
                        WHITE);
    EndShaderMode();
}


/**
 * @brief Render the screen in graphics mode
 */
static void zvb_render_gfx_mode(zvb_t* zvb)
{
    zvb_shader_t* st_shader = &zvb->shaders[SHADER_GFX];
    const Shader shader = st_shader->shader;

    const int mode_idx     = st_shader->objects[GFX_SHADER_VIDMODE_IDX];
    const int tilemaps_idx = st_shader->objects[GFX_SHADER_TILEMAPS_IDX];
    const int tileset_idx  = st_shader->objects[GFX_SHADER_TILESET_IDX];
    const int sprites_idx  = st_shader->objects[GFX_SHADER_SPRITES_IDX];
    const int scroll0_idx  = st_shader->objects[GFX_SHADER_SCROLL0_IDX];
    const int scroll1_idx  = st_shader->objects[GFX_SHADER_SCROLL1_IDX];
    const int palette_idx  = st_shader->objects[GFX_SHADER_PALETTE_IDX];

    BeginShaderMode(shader);
        zvb_palette_update(&zvb->palette, &st_shader->shader, palette_idx);
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


static void zvb_render_debug_gfx_mode(zvb_t* zvb)
{
    /* Since we want to generate a debug texture, we only need to set it to debug mode */
    zvb_shader_t* st_shader = &zvb->shaders[SHADER_GFX_DEBUG];
    const Shader shader = st_shader->shader;
    const int mode_idx     = st_shader->objects[GFX_SHADER_VIDMODE_IDX];
    const int dbg_mode_idx = st_shader->objects[GFX_SHADER_DBGMODE_IDX];
    const int tilemaps_idx = st_shader->objects[GFX_SHADER_TILEMAPS_IDX];
    const int tileset_idx  = st_shader->objects[GFX_SHADER_TILESET_IDX];
    const int palette_idx  = st_shader->objects[GFX_SHADER_PALETTE_IDX];

    int dbg_mode = GFX_DEBUG_LAYER0_MODE;
    RenderTexture* texture = &zvb->debug_tex[DBG_TILEMAP_LAYER0];

    BeginTextureMode(*texture);
        BeginShaderMode(shader);
            /* Transfer all the texture to the GPU */
            zvb_palette_force_update(&zvb->palette, &st_shader->shader, palette_idx);
            SetShaderValue(shader, mode_idx, &zvb->mode, SHADER_UNIFORM_INT);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            SetShaderValueTexture(shader, tilemaps_idx, *zvb_tilemap_texture(&zvb->layers));
            SetShaderValueTexture(shader, tileset_idx, *zvb_tileset_texture(&zvb->tileset));

            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Debug layer 1, tell the shader to debug layer 1 in the mode (bit 30) */
    dbg_mode = GFX_DEBUG_LAYER1_MODE;
    texture = &zvb->debug_tex[DBG_TILEMAP_LAYER1];
    BeginTextureMode(*texture);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Debug the tileset, in this case, we have 16x32 tiles at most */
    dbg_mode = GFX_DEBUG_TILESET_MODE;
    texture = &zvb->debug_tex[DBG_TILESET];
    BeginTextureMode(*texture);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();

    /* Palette mode */
    dbg_mode = GFX_DEBUG_PALETTE_MODE;
    texture = &zvb->debug_tex[DBG_PALETTE];
    BeginTextureMode(*texture);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
            SetShaderValue(shader, dbg_mode_idx, &dbg_mode, SHADER_UNIFORM_INT);
            DrawTextureRec(zvb->tex_dummy.texture,
                            (Rectangle){ 0, 0, texture->texture.width, texture->texture.height },
                            (Vector2){ 0, 0 },
                            WHITE);
        EndShaderMode();
    EndTextureMode();
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

void zvb_render_debug_textures(zvb_t* zvb)
{
    switch (zvb->mode) {
        case MODE_TEXT_640:
        case MODE_TEXT_320:
            zvb_render_debug_text_mode(zvb);
            break;

        case MODE_BITMAP_256:
        case MODE_BITMAP_320:
            // zvb_render_debug_bitmap_mode(zvb);
            break;

        default:
            zvb_render_debug_gfx_mode(zvb);
            break;
    }
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
        switch (zvb->mode) {
            case MODE_TEXT_640:
            case MODE_TEXT_320:
                zvb_render_text_mode(zvb);
                break;

            case MODE_BITMAP_256:
            case MODE_BITMAP_320:
                zvb_render_bitmap_mode(zvb);
                break;

            default:
                zvb_render_gfx_mode(zvb);
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
    for (int i = 0; i < DBG_VIEW_TOTAL; i++) {
        UnloadRenderTexture(zvb->debug_tex[i]);
    }
}
