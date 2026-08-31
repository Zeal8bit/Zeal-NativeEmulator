/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "app/console/console.h"

#if CONFIG_ENABLE_DEBUGGER

/* Debugger command handlers, defined in console_debug.c and referenced by the
 * console dispatch table (console.c). */
void console_debug_continue(zeal_t* machine, int argc, char** argv);
void console_debug_step(zeal_t* machine, int argc, char** argv);
void console_debug_step_over(zeal_t* machine, int argc, char** argv);
void console_debug_pause(zeal_t* machine, int argc, char** argv);
void console_debug_bp(zeal_t* machine, int argc, char** argv);
void console_debug_bc(zeal_t* machine, int argc, char** argv);
void console_debug_bl(zeal_t* machine, int argc, char** argv);
void console_debug_regs(zeal_t* machine, int argc, char** argv);
void console_debug_rb(zeal_t* machine, int argc, char** argv);
void console_debug_r16(zeal_t* machine, int argc, char** argv);
void console_debug_r32(zeal_t* machine, int argc, char** argv);
void console_debug_wb(zeal_t* machine, int argc, char** argv);
void console_debug_disasm(zeal_t* machine, int argc, char** argv);
void console_debug_mmu(zeal_t* machine, int argc, char** argv);

#endif /* CONFIG_ENABLE_DEBUGGER */
