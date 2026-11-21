/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Monotonic UI clock. */
#pragma once

namespace zui::common {

/** Seconds since the UI started (steady clock). */
double now_seconds();

} // namespace zui::common
