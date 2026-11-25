/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 * SPDX-License-Identifier: Apache-2.0
 */
#include "common/rpu_disasm.h"
#include <cstdio>

namespace zui::common {

void rpu_decode(uint32_t ins, char* mnemonic, size_t mnemonic_size,
                char* operands, size_t operands_size)
{
    const unsigned vpos = (ins >> 7) & 0x1ff;
    const unsigned hpos = (ins & 0x7f) << 3;
    if (((ins >> 22) & 0x3) == 2) {
        snprintf(mnemonic, mnemonic_size, "LOAD");
        snprintf(operands, operands_size, "$%04X <- $%02X",
                 (unsigned) ((ins >> 8) & 0x3fff), (unsigned) (ins & 0xff));
        return;
    }
    switch ((ins >> 16) & 0xff) {
        case 0x00:
            snprintf(mnemonic, mnemonic_size, "WAIT");
            snprintf(operands, operands_size, "v=%u h=%u", vpos, hpos);
            break;
        case 0x20:
            snprintf(mnemonic, mnemonic_size, "JUMP");
            snprintf(operands, operands_size, "$%02X", (unsigned) (ins & 0xff));
            break;
        case 0x40:
            snprintf(mnemonic, mnemonic_size, "SKIP");
            snprintf(operands, operands_size, "v=%u h=%u", vpos, hpos);
            break;
        case 0x60:
            snprintf(mnemonic, mnemonic_size, "MASK");
            snprintf(operands, operands_size, "V=$%03X H=$%03X", vpos, hpos);
            break;
        default:
            snprintf(mnemonic, mnemonic_size, "???");
            snprintf(operands, operands_size, "$%06X", (unsigned) (ins & 0xffffff));
            break;
    }
}

} // namespace zui::common
