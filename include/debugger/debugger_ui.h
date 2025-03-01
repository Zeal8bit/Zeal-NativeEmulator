#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "debugger/debugger.h"

struct dbg_ui_t;

int debugger_ui_init(struct dbg_ui_t** ret_ctx, RenderTexture2D* emu_view);

void debugger_ui_deinit(struct dbg_ui_t* ctx);

void debugger_ui_prepare_render(struct dbg_ui_t* ctx, dbg_t* dbg);

void debugger_ui_render(struct dbg_ui_t* ctx, dbg_t* dbg);

bool debugger_ui_main_view_focused(const struct dbg_ui_t* ctx);
