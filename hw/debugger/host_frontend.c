/* SPDX-License-Identifier: Apache-2.0 */
#include "host_frontend.h"
#include "platform/display.h"
#include "hw/userport/snes_adapter/controller.h"
#include "hw/zeal.h"
#include "utils/notif.h"
#include "utils/paths.h"
#include <math.h>
#include <string.h>
#include "platform/audio.h"

static void host_action(dbg_t *dbg, dbg_host_action_t op, int32_t a, int32_t b)
{
    zeal_t *m = dbg->arg;
    switch (op) {
    case UI_SNES_PORT:
        if (b == DBG_SNES_DETACHED)
            snes_adapter_assign_port(&m->snes_adapter, a, SNES_PORT_DEVICE_DETACHED, 0);
        else if (b == DBG_SNES_MOUSE)
            snes_adapter_assign_port(&m->snes_adapter, a, SNES_PORT_DEVICE_MOUSE, 0);
        else if (b >= 0 && b < SNES_GAMEPAD_COUNT && snes_controller_available(b))
            snes_adapter_assign_port(&m->snes_adapter, a, SNES_PORT_DEVICE_CONTROLLER, b);
        break;
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
        audio_set_volume(config.audio.volume / 100.0f);
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

static void host_snes_state(dbg_t *dbg, dbg_snes_state_t *out)
{
    snes_adapter_t *adapter = &((zeal_t *)dbg->arg)->snes_adapter;
    memset(out, 0, sizeof(*out));
    for (unsigned i = 0; i < DBG_SNES_PORTS; ++i) {
        const snes_port_assignment_t *port = &adapter->ports[i];
        out->ports[i] = port->device == SNES_PORT_DEVICE_MOUSE        ? DBG_SNES_MOUSE
                        : port->device == SNES_PORT_DEVICE_CONTROLLER ? port->controller_index
                                                                      : DBG_SNES_DETACHED;
    }
    for (unsigned i = 0; i < DBG_HOST_GAMEPADS; ++i) {
        out->gamepads[i].available = snes_controller_available(i);
        const char *name = out->gamepads[i].available ? snes_controller_name(i) : NULL;
        snprintf(out->gamepads[i].name, sizeof(out->gamepads[i].name), "%s",
                 name ? name : "Disconnected gamepad");
    }
}

static int host_font_atlas(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16])
{
    /* Rasterizing the CP437 font is a host capability: FLTK uses FreeType, the
     * WebAssembly host uses Raylib. See include/platform/display.h. */
    return display_font_atlas(codepoints, alpha) ? 1 : 0;
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
    args->snes_state = host_snes_state;
    const char *keys[] = {"P_VIDEO",  "P_CPU", "P_BREAKPOINTS", "P_DISASSEMBLER",
                          "P_MEMORY", "P_MMU", "P_SEMIHOST",    "P_VRAM"};
    for (unsigned i = 0; i < 8; ++i) {
        char key[64];
        snprintf(key, sizeof(key), "%s_HIDDEN", keys[i]);
        if (config_get(key, i == 7))
            args->hidden_panels |= 1u << i;
    }
}
