/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The C side of the frontend as seen from the C++ UI: bridge API, debugger
 * types and functions, configuration and notifications. The UI never
 * includes hw/zeal.h (its <stdatomic.h> is not C++ on every compiler). */
#pragma once

extern "C" {
#include "frontend/sdl-fltk/bridge/machine.h"
#include "debugger/debugger.h"
#include "frontend/common/panels.h"
#include "utils/config.h"
#include "utils/notif.h"
}
