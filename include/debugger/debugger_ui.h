/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "debugger/debugger.h"
#include "ui/raylib-nuklear.h"
#include "utils/config.h"
/* Workaround to have access to more info of the VRAM */
#include "hw/zvb/zvb.h"

#define WIN_UI_FONT_SIZE        13

#define MENUBAR_HEIGHT          30
#define NK_WIDGET_TITLE_HEIGHT  50

#define CPU_CTRL_WIDTH          250
#define CPU_CTRL_HEIGHT         280
#define CPU_CTRL_REG_HEIGHT     25
#define CPU_CTRL_REG_PADDING    0.1f

#define MOUSE_DEFAULT   MOUSE_CURSOR_DEFAULT
#define MOUSE_POINTER   MOUSE_CURSOR_POINTING_HAND
#define MOUSE_TEXT      MOUSE_CURSOR_IBEAM

#define DBG_MAX_VRAM_VIEWS      5

typedef enum {
    DBG_UI_PANEL_VIDEO,
    DBG_UI_PANEL_BKPOINT,
    DBG_UI_PANEL_CPU,
    DBG_UI_PANEL_MEMORY,
    DBG_UI_PANEL_DISASSEMBLER,
    DBG_UI_PANEL_VRAM,
    DBG_UI_PANEL_MMU,
    DBG_UI_PANEL_TOTAL
} dbg_ui_panels_idx_t;


typedef struct dbg_ui_panel_t dbg_ui_panel_t;
struct dbg_ui_t {
    struct nk_context* ctx;
    struct nk_image    view;
    hwaddr             mem_view_addr;
    hwaddr             mem_view_size;
    hwaddr             dis_addr;
    hwaddr             dis_size;
    struct nk_image    vram[DBG_MAX_VRAM_VIEWS];
    zvb_t*             zvb;
};

typedef void (*dbg_ui_panel_fn)(struct dbg_ui_panel_t*, struct dbg_ui_t*, dbg_t*);

struct dbg_ui_panel_t {
    const char* key;
    const char* title;
    struct nk_rect rect;
    struct nk_rect rect_default;
    nk_flags flags;
    bool hidden;
    const bool hidden_default;
    dbg_ui_panel_fn render;

    bool override_header_style;
    struct nk_style_item header_style;
};

typedef struct {
    const RenderTexture2D* main_view;
    const RenderTexture2D* debug_views;
    int debug_views_count;
    zvb_t* zvb;
} dbg_ui_init_args_t;

extern char DEBUG_BUFFER[256];

void ui_menubar(struct dbg_ui_t* dctx, dbg_t* dbg, dbg_ui_panel_t *panels, int panels_size);

void ui_panel_display(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg);
void debugger_scale_up(dbg_t *dbg);
void debugger_scale_down(dbg_t *dbg);

/* Debug Panels */
void ui_panel_cpu(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg);
void ui_panel_breakpoints(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg);
void ui_panel_memory(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg);
void ui_panel_mmu(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg);
void ui_panel_disassembler(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg);
void ui_panel_vram(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg);

int debugger_ui_init(struct dbg_ui_t** ret_ctx, const dbg_ui_init_args_t* args);
void debugger_ui_deinit(struct dbg_ui_t* dctx);
int dbg_ui_config_save(rini_config *config);

void debugger_ui_prepare_render(struct dbg_ui_t* dctx, dbg_t* dbg);
void debugger_ui_render(struct dbg_ui_t* dctx, dbg_t* dbg);
bool debugger_ui_main_view_focused(const struct dbg_ui_t* dctx);
bool debugger_ui_vram_panel_opened(const struct dbg_ui_t* dctx);

/** Helpers */
bool dbg_ui_clickable_label(struct nk_context* ctx, const char* label, const char* value, bool active);
void dbg_ui_mouse_hover(struct nk_context *ctx, int cursor);
void dbg_ui_update_cursor(struct nk_context *ctx, struct nk_rect rect);
void dbg_ui_byte_to_hex(uint8_t byte, char* out, char separator);
void dbg_ui_word_to_hex(uint16_t word, char* out, char separator);
void dbg_ui_go_to_mem(struct dbg_ui_t* dctx, hwaddr addr);
