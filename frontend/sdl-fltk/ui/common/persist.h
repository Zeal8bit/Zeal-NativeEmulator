/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Extra configuration keys written by config_save(). */
#pragma once

#include <functional>
#include "common/machine.h"

namespace zui::common {

/** Register a function that appends keys (use an FLTK_ prefix) when the
 *  configuration is saved. Read values back with config_get/_text. */
void persist_register(std::function<void(rini_config*)> save);

} // namespace zui::common
