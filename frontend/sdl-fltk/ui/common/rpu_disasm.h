/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace zui::common {

/* Decode one 24-bit RPU instruction into a mnemonic and operands. */
void rpu_decode(uint32_t instruction, char* mnemonic, size_t mnemonic_size,
                char* operands, size_t operands_size);

} // namespace zui::common
