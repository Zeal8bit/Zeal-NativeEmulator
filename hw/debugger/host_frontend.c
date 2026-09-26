/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zeal.h"
#include "utils/notif.h"
#include "utils/paths.h"
#include <math.h>
#include <string.h>

static void host_action(dbg_t *dbg, dbg_host_action_t op, int32_t a, int32_t b)
{
    zeal_t *m = dbg->arg;
    switch (op) {
    case UI_ON:
        zeal_debug_enable(m);
        break;
    case UI_OFF:
        zeal_debug_disable(m);
        break;
    case UI_SAVE:
        config_save();
        break;
    case UI_VOLUME:
        config.audio.volume = a < 0 ? 0 : a > 100 ? 100 : a;
        SetMasterVolume(config.audio.volume / 100.0f);
        break;
    case UI_PASSTHROUGH:
        config.debugger.keyboard_passthru = a != 0;
        break;
    case UI_MOUSE_PORT:
        snes_adapter_set_mouse_port(&m->snes_adapter, a);
        break;
    case UI_CONTROLLER_PORT:
        snes_adapter_set_controller_port(&m->snes_adapter, a, b);
        break;
    case UI_MOUSE_SPEED:
        m->snes_adapter.mouse.delta_scale =
            fminf(SNES_MOUSE_DELTA_SCALE_MAX,
                  fmaxf(SNES_MOUSE_DELTA_SCALE_MIN,
                        m->snes_adapter.mouse.delta_scale + a * SNES_MOUSE_DELTA_SCALE_STEP));
        break;
    case UI_MOUSE_RESET:
        snes_adapter_reset_mouse_scale(&m->snes_adapter);
        break;
    }
}
static void host_key(dbg_t *dbg, uint32_t key, uint32_t down)
{
    zeal_t *m = dbg->arg;
    if (key >= 384 || key == 0)
        return;
    if (down) {
        if (!debugger_is_paused(dbg)) {
            key_pressed(&m->keyboard, key);
            m->frontend_keys[key] = true;
        }
    } else if (m->frontend_keys[key]) {
        key_released(&m->keyboard, key);
        m->frontend_keys[key] = false;
    }
}
static void host_release(dbg_t *dbg)
{
    zeal_t *m = dbg->arg;
    for (unsigned k = 0; k < 384; ++k)
        if (m->frontend_keys[k])
            host_key(dbg, k, 0);
    m->frontend_mouse_dx = m->frontend_mouse_dy = 0;
    m->frontend_mouse_buttons = 0;
}
static void host_mouse(dbg_t *dbg, int32_t dx, int32_t dy, uint32_t buttons)
{
    zeal_t *m = dbg->arg;
    m->frontend_mouse_dx += dx;
    m->frontend_mouse_dy += dy;
    m->frontend_mouse_buttons = buttons;
}
static void host_notification(char *out, uint32_t capacity)
{
    if (out && capacity)
        snprintf(out, capacity, "%s", notif_text());
}

static int host_font_atlas(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16])
{
    char path[PATH_MAX];
    get_install_dir_file(path, "assets/fonts/BigBlue_Terminal_437TT.TTF");
    if (!FileExists(path))
        snprintf(path, sizeof(path), "assets/fonts/BigBlue_Terminal_437TT.TTF");
#ifdef ZEAL_ASSETS_DIR
    if (!FileExists(path))
        snprintf(path, sizeof(path), "%s/fonts/BigBlue_Terminal_437TT.TTF", ZEAL_ASSETS_DIR);
#endif
    int size = 0;
    unsigned char *data = LoadFileData(path, &size);
    if (!data)
        return 0;
    int codes[256];
    for (unsigned i = 0; i < 256; i++)
        codes[i] = (int)codepoints[i];
    GlyphInfo *glyphs = LoadFontData(data, size, 16, codes, 256, FONT_BITMAP);
    UnloadFileData(data);
    if (!glyphs)
        return 0;
    memset(alpha, 0, 256 * 8 * 16);
    for (unsigned i = 0; i < 256; i++) {
        Image *image = &glyphs[i].image;
        bool mask = image->format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        ImageFormat(image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        for (int y = 0; y < image->height; y++)
            for (int x = 0; x < image->width; x++) {
                int dx = x + glyphs[i].offsetX, dy = y + glyphs[i].offsetY;
                if (dx >= 0 && dx < 8 && dy >= 0 && dy < 16)
                    alpha[i * 128 + dy * 8 + dx] =
                        ((uint8_t *)image->data)[(y * image->width + x) * 4 + (mask ? 0 : 3)];
            }
    }
    UnloadFontData(glyphs, 256);
    return 1;
}
void debugger_host_frontend_args(dbg_t *dbg, dbg_ui_init_args_t *args)
{
    *args = (dbg_ui_init_args_t){.debugger = dbg,
                                 .config_directory = get_config_dir(),
                                 .width = config.debugger.width,
                                 .height = config.debugger.height,
                                 .volume = config.audio.volume,
                                 .passthrough = config.debugger.keyboard_passthru,
                                 .action = host_action,
                                 .key = host_key,
                                 .release_input = host_release,
                                 .mouse = host_mouse,
                                 .font_atlas = host_font_atlas,
                                 .notification = host_notification};
    const char *keys[] = {"P_VIDEO",  "P_CPU", "P_BREAKPOINTS", "P_DISASSEMBLER",
                          "P_MEMORY", "P_MMU", "P_SEMIHOST",    "P_VRAM"};
    for (unsigned i = 0; i < 8; ++i) {
        char key[64];
        snprintf(key, sizeof(key), "%s_HIDDEN", keys[i]);
        if (config_get(key, i == 7))
            args->hidden_panels |= 1u << i;
    }
}
