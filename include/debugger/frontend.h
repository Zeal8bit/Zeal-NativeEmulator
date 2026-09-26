/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "api.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct dbg_ui_t dbg_ui_t;
typedef enum {
    UI_OFF,
    UI_SAVE,
    UI_VOLUME,
    UI_PASSTHROUGH,
    UI_MOUSE_PORT,
    UI_CONTROLLER_PORT,
    UI_MOUSE_RESET,
    UI_MOUSE_SPEED,
    UI_ON,
    UI_SNES_PORT
} dbg_host_action_t;
enum { DBG_SNES_PORTS = 2, DBG_HOST_GAMEPADS = 4, DBG_SNES_DETACHED = -1, DBG_SNES_MOUSE = -2 };
typedef struct {
    /* Each port selects detached, mouse, or a host gamepad index. */
    int32_t ports[DBG_SNES_PORTS];
    struct {
        uint32_t available;
        char name[128];
    } gamepads[DBG_HOST_GAMEPADS];
} dbg_snes_state_t;
typedef struct {
    dbg_t *debugger;
    const char *config_directory;
    uint32_t hidden_panels;
    int32_t width, height, volume, passthrough;
    void (*action)(dbg_t *, dbg_host_action_t, int32_t, int32_t);
    void (*key)(dbg_t *, uint32_t key, uint32_t down);
    void (*release_input)(dbg_t *);
    void (*notification)(char *output, uint32_t capacity);
    int (*font_atlas)(const uint32_t codepoints[256], uint8_t alpha[256 * 8 * 16]);
    void (*mouse)(dbg_t *, int32_t dx, int32_t dy, uint32_t buttons);
    void (*snes_state)(dbg_t *, dbg_snes_state_t *);
} dbg_ui_init_args_t;
int debugger_ui_init(dbg_ui_t **, const dbg_ui_init_args_t *);
void debugger_ui_deinit(dbg_ui_t *);
void debugger_ui_show(dbg_ui_t *, bool);
void debugger_ui_poll(dbg_ui_t *);
void debugger_ui_refresh(dbg_ui_t *);
bool debugger_ui_main_view_focused(const dbg_ui_t *);
dbg_vram_t debugger_ui_vram_panel_opened(const dbg_ui_t *);
void debugger_ui_scale(dbg_ui_t *, int delta);
#ifdef __cplusplus
}
#endif
