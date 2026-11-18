/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Fonts: the BigBlue Terminal pixel font for data views (registered from
 * the assets at runtime) and the system monospace font for the UI. */
#pragma once

namespace zui::theme {

/** Register the data font and pick DATA_FONT/DATA_SIZE. */
void load_fonts();
/** System monospace font for all UI text (replaces Helvetica). */
void apply_ui_font();

} // namespace zui::theme
