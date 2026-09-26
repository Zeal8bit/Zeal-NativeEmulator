/* SPDX-License-Identifier: Apache-2.0 */
#include "bindings_internal.h"
#include "debugger_internal.h"
#include <string.h>

static dbg_t *active;
static uint32_t generation;
uint32_t debugger_bind(dbg_t *dbg)
{
    if (!dbg)
        return 0;
    active = dbg;
    if (++generation == 0)
        ++generation;
    return generation;
}
void debugger_unbind(dbg_t *dbg)
{
    if (active == dbg)
        active = NULL;
}
dbg_t *debugger_resolve(uint32_t handle) { return handle && handle == generation ? active : NULL; }
DBG_EXPORT uint32_t zeal_debug_handle(void) { return active ? generation : 0; }
DBG_EXPORT uint32_t zeal_debug_command(uint32_t h, uint32_t command)
{
    if (command > DBG_STOP)
        return DBG_INVALID;
    return debugger_command(debugger_resolve(h), (dbg_command_t)command);
}
DBG_EXPORT uint32_t zeal_debug_state(uint32_t h, uint8_t *out, uint32_t capacity)
{
    dbg_snapshot_t s;
    dbg_status_t status = debugger_snapshot(debugger_resolve(h), &s);
    if (status != DBG_OK)
        return status;
    if (capacity < 24)
        return DBG_CAPACITY;
    if (!out)
        return DBG_INVALID;
    dbg_put32(out, s.paused);
    dbg_put32(out + 4, s.stopped);
    dbg_put32(out + 8, s.reason);
    dbg_put32(out + 12, s.pc);
    dbg_put64(out + 16, s.sequence);
    return DBG_OK;
}
DBG_EXPORT uint32_t zeal_debug_events(uint32_t h, uint32_t low, uint32_t high, uint8_t *out,
                                      uint32_t capacity)
{
    dbg_t *d = debugger_resolve(h);
    if (!d)
        return DBG_INVALID;
    if (capacity < 8)
        return DBG_CAPACITY;
    if (!out)
        return DBG_INVALID;
    dbg_event_record_t events[64];
    uint32_t count, overflow;
    uint32_t available = (capacity - 8) / 20;
    if (available > 64)
        available = 64;
    dbg_status_t s = debugger_events(d, ((uint64_t)high << 32) | low, events, available, &count, &overflow);
    if (s != DBG_OK && s != DBG_CAPACITY)
        return s;
    dbg_put32(out, count);
    dbg_put32(out + 4, overflow);
    if (available)
        for (uint32_t i = 0; i < count; i++) {
            uint8_t *p = out + 8 + i * 20;
            dbg_put64(p, events[i].sequence);
            dbg_put32(p + 8, events[i].reason);
            dbg_put32(p + 12, events[i].address);
            dbg_put32(p + 16, events[i].access);
        }
    return s;
}
DBG_EXPORT uint32_t zeal_debug_memory(uint32_t h, uint32_t write, uint32_t physical, uint32_t address,
                                      uint8_t *bytes, uint32_t count)
{
    if (write > 1 || physical > 1)
        return DBG_INVALID;
    return write ? debugger_memory_write(debugger_resolve(h), (dbg_address_space_t)physical, address, bytes,
                                         count)
                 : debugger_memory_read(debugger_resolve(h), (dbg_address_space_t)physical, address, bytes,
                                        count);
}
DBG_EXPORT uint32_t zeal_debug_registers(uint32_t h, uint32_t write, uint8_t *bytes, uint32_t capacity)
{
    dbg_t *d = debugger_resolve(h);
    if (!d || write > 1)
        return DBG_INVALID;
    if (capacity < 26)
        return DBG_CAPACITY;
    if (!bytes)
        return DBG_INVALID;
    if (!d->get_regs_cb || (write && !d->set_regs_cb))
        return DBG_UNAVAILABLE;
    regs_t regs = {0};
    debugger_get_registers(d, &regs);
    uint16_t *fields[] = {&regs.pc,  &regs.sp,  &regs.af,  &regs.bc, &regs.de, &regs.hl, &regs.af_,
                          &regs.bc_, &regs.de_, &regs.hl_, &regs.ix, &regs.iy, &regs.ir};
    for (unsigned i = 0; i < 13; i++)
        if (write)
            *fields[i] = (uint16_t)(bytes[2 * i] | ((uint16_t)bytes[2 * i + 1] << 8));
        else {
            bytes[2 * i] = *fields[i] & 255;
            bytes[2 * i + 1] = *fields[i] >> 8;
        }
    if (write)
        debugger_set_registers(d, &regs);
    return DBG_OK;
}
DBG_EXPORT uint32_t zeal_debug_breakpoint(uint32_t h, uint32_t addr, uint32_t enabled)
{
    dbg_t *d = debugger_resolve(h);
    if (!d || enabled > 1)
        return DBG_INVALID;
    if (addr > 65535)
        return DBG_RANGE;
    bool set = debugger_is_breakpoint_set(d, addr);
    if (set == !!enabled)
        return DBG_OK;
    return (enabled ? debugger_set_breakpoint(d, addr) : debugger_clear_breakpoint(d, addr)) ? DBG_OK
                                                                                             : DBG_CAPACITY;
}
DBG_EXPORT uint32_t zeal_debug_watchpoint(uint32_t h, uint32_t addr, uint32_t access)
{
    dbg_t *d = debugger_resolve(h);
    if (!d || access > 3)
        return DBG_INVALID;
    if (addr > 65535)
        return DBG_RANGE;
    if (!access) {
        debugger_remove_watchpoint(d, addr);
        return DBG_OK;
    }
    return debugger_add_watchpoint(d, (watchpoint_t){addr, (watchpoint_type_t)access}) ? DBG_OK
                                                                                       : DBG_CAPACITY;
}
DBG_EXPORT uint32_t zeal_debug_points(uint32_t h, uint32_t watches, uint8_t *out, uint32_t capacity)
{
    dbg_t *d = debugger_resolve(h);
    if (!d || watches > 1)
        return DBG_INVALID;
    if (capacity < 4)
        return DBG_CAPACITY;
    if (!out)
        return DBG_INVALID;
    hwaddr b[DBG_MAX_POINTS];
    watchpoint_t w[DBG_MAX_POINTS];
    uint32_t n = watches ? debugger_get_watchpoints(d, w, DBG_MAX_POINTS)
                         : debugger_get_breakpoints(d, b, DBG_MAX_POINTS);
    dbg_put32(out, n);
    if (capacity < 4 + n * 8)
        return DBG_CAPACITY;
    for (uint32_t i = 0; i < n; i++) {
        dbg_put32(out + 4 + i * 8, watches ? w[i].addr : b[i]);
        dbg_put32(out + 8 + i * 8, watches ? (uint32_t)w[i].type : 0);
    }
    return DBG_OK;
}
DBG_EXPORT uint32_t zeal_debug_symbols(uint32_t h, const char *text, uint32_t length)
{
    return debugger_symbols_text(debugger_resolve(h), text, length);
}
DBG_EXPORT uint32_t zeal_debug_disassemble(uint32_t h, uint32_t address, uint8_t *out, uint32_t capacity)
{
    dbg_t *d = debugger_resolve(h);
    if (!d)
        return DBG_INVALID;
    if (address > 65535)
        return DBG_RANGE;
    if (capacity < 136)
        return DBG_CAPACITY;
    if (!out)
        return DBG_INVALID;
    dbg_instr_t in = {0};
    int n = debugger_disassemble_address(d, address, &in);
    if (n < 1)
        return DBG_UNAVAILABLE;
    dbg_put32(out, n);
    memcpy(out + 4, in.opcodes, 4);
    memcpy(out + 8, in.label, 64);
    memcpy(out + 72, in.instruction, 64);
    return DBG_OK;
}
