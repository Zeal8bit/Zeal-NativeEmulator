/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Host (FLTK) key codes to emulated keyboard keys. */
#pragma once

namespace zui::app {

/** Convert an FLTK key code to a zeal_key_t, or 0 (ZEAL_KEY_NULL). */
int fltk_to_zeal_key(int fltk_key);

} // namespace zui::app
