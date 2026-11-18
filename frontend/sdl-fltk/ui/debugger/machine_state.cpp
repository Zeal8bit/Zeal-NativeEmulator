/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "debugger/machine_state.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "common/machine.h"
#include "debugger/address_lookup.h"

namespace zui::debugger {

bool dbg_ready()
{
    return fz_debugger() != nullptr;
}

regs_t current_regs()
{
    regs_t regs;
    memset(&regs, 0, sizeof(regs));
    if (!dbg_ready()) return regs;
    if (!fz_paused() && fz_live_sample(&regs, nullptr, 0, nullptr, 0)) return regs;
    debugger_get_registers(fz_debugger(), &regs);
    return regs;
}

regs_t exact_regs()
{
    regs_t regs;
    memset(&regs, 0, sizeof(regs));
    if (dbg_ready()) debugger_get_registers(fz_debugger(), &regs);
    return regs;
}

void display_memory(hwaddr addr, int len, uint8_t* out, bool stack)
{
    if (!fz_paused()) {
        const bool ok = stack ? fz_live_sample(nullptr, out, len, nullptr, 0)
                              : fz_live_sample(nullptr, nullptr, 0, out, len);
        if (ok) return;
    }
    if (dbg_ready()) debugger_read_memory(fz_debugger(), addr, len, out);

}

} // namespace zui::debugger
