/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Frontend keys written by config_save(), as user or project settings. */
#pragma once

#include <functional>
#include "common/machine.h"

namespace zui::common {

/* Configuration is split in two files (frontend/common/config.c):
 * - user settings (~/.zeal8bit/zeal.ini): shared by every program, e.g.
 *   window geometry, layout, tool windows, audio, display preferences;
 * - project settings (./zeal.ini where the emulator is launched): what
 *   belongs to the program being developed, e.g. breakpoints, watchpoints,
 *   watches, map files.
 * Reading (config_get/_text) sees both, the project first. */

/** Register a function that appends user settings (use an FLTK_ prefix)
 *  when the configuration is saved. */
void persist_register(std::function<void(rini_config*)> save);
/** Same for project settings. */
void persist_register_project(std::function<void(rini_config*)> save);

/** The UI is closing: run every callback once into a snapshot, drop the
 *  callbacks (their owners, often widgets, are about to be destroyed) and
 *  make later saves write that snapshot. config_save() runs after the UI
 *  is gone, so this is what keeps the last values without dangling
 *  captures. Registering again (a new UI) discards the snapshot. */
void persist_finalize();

} // namespace zui::common
