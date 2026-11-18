/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <functional>

namespace zui::memtools {

/** Snapshots the changed-bytes search compares (owned by the memory view). */
struct SnapshotSource {
    /** Previous and current pause images (64 KB each). False before the
     *  first pause; `prev` is null while only one pause has been seen. */
    std::function<bool(const uint8_t** prev, const uint8_t** cur)> get;
    std::function<void(unsigned addr)> go;
};

} // namespace zui::memtools
