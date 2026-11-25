/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdio.h>
#include "debugger/debugger.h"
bool debugger_load_symbols(dbg_t*, const char* filename, bool append);

/* The caller owns the stream. */
bool debugger_load_symbols_stream(dbg_t*, FILE*, const char* name, bool append);
