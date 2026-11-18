/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The Zeal character set (code page 437) as Unicode. */
#pragma once

#include <cstdint>

namespace zui::common {

/** Unicode code point of a CP437 byte (0 for NUL). */
uint16_t cp437_to_unicode(uint8_t byte);

} // namespace zui::common
