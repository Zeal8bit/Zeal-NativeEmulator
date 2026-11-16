/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "raylib.h"
#include "hw/zeal.h"
#define WIN_NAME "Zeal 8-bit Computer"
#define WIN_LOG_LEVEL LOG_WARNING
extern struct dbg_ui_t* nuklear_ui;
#if CONFIG_ENABLE_DEBUGGER
#include "frontend/nuklear/debugger_ui.h"
/**
 * @brief Enable the Zeal Debugger view.
 */
int  zeal_debug_enable(zeal_t*);
/**
 * @brief Disable the Zeal Debugger view.
 */
int  zeal_debug_disable(zeal_t*);
/**
 * @brief Toggle the Zeal Debugger view.
 */
void zeal_debug_toggle(dbg_t*);
#endif
/**
 * @brief Quantize a scale factor to the next or previous 10% step.
 */
float zeal_scale_quantize_tenths(float scale, float step);
