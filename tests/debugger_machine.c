#include "hw/debugger/bindings_internal.h"
#include "hw/zeal.h"
#include <assert.h>
#include <string.h>
static zeal_t machine;
int main(void)
{
    config.arguments.headless = true;
    config.arguments.console = true;
    config.debugger.enabled = DEBUGGER_STATE_ARG;
    assert(zeal_init(&machine) == 0);
    dbg_t *d = &machine.dbg;
    mmu_io_write_byte(&machine.cpu.mmu, 0xf1, 32);
    uint8_t program[] = {0x3e, 0x42, 0x32, 0x20, 0x40, 0x00, 0xc3, 0x00, 0x40};
    assert(debugger_memory_write(d, DBG_VIRTUAL, 0x4000, program, sizeof(program)) == DBG_OK);
    uint8_t copy[9];
    assert(debugger_memory_read(d, DBG_PHYSICAL, 0x080000, copy, 9) == DBG_OK && !memcmp(program, copy, 9));
    regs_t r = {0};
    r.pc = 0x4000;
    r.sp = 0x4100;
    r.a_ = 0x55;
    r.a = 0x22;
    debugger_set_registers(d, &r);
    regs_t check;
    debugger_get_registers(d, &check);
    assert(check.a_ == 0x55 && check.a == 0x22);
    assert(debugger_command(d, DBG_STEP) == DBG_OK);
    assert(zeal_debugger_run(&machine, 100));
    debugger_get_registers(d, &check);
    assert(check.pc == 0x4002 && check.a == 0x42);
    assert(debugger_add_watchpoint(d, (watchpoint_t){0x4020, WATCHPOINT_WRITE}));
    debugger_continue(d);
    assert(zeal_debugger_run(&machine, 100));
    dbg_snapshot_t snap;
    debugger_snapshot(d, &snap);
    assert(snap.reason == DBG_REASON_WATCHPOINT && snap.pc == 0x4005);
    assert(debugger_memory_read(d, DBG_VIRTUAL, 0x4020, copy, 1) == DBG_OK && copy[0] == 0x42);
    assert(debugger_remove_watchpoint(d, 0x4020));
    assert(debugger_set_breakpoint(d, 0x4000));
    debugger_continue(d);
    assert(zeal_debugger_run(&machine, 100));
    debugger_snapshot(d, &snap);
    assert(snap.reason == DBG_REASON_BREAKPOINT && snap.pc == 0x4000);
    dbg_mapping_t maps[4];
    assert(debugger_mappings(d, maps) == DBG_OK && maps[1].physical_address == 0x080000);
    dbg_counter_t counters[8];
    assert(debugger_counter_break(d, 2, 1) == DBG_OK);
    assert(debugger_counters(d, counters) == DBG_OK && counters[2].break_on_update);
    assert(debugger_counter_break(d, 8, 1) == DBG_RANGE);
    dbg_image_info_t info;
    assert(debugger_image_copy(d, -1, &info, 0, 0) == DBG_UNAVAILABLE);
    dbg_instr_t in;
    assert(debugger_disassemble_address(d, 0x4000, &in) == 2 && in.size == 2);
    uint32_t h = zeal_debug_handle();
    assert(h);
    uint8_t encoded[512];
    assert(zeal_debug_registers(h, 0, encoded, 26) == DBG_OK);
    assert(encoded[0] == 0 && encoded[1] == 0x40);
    assert(zeal_debug_mappings(h, encoded, sizeof(encoded)) == DBG_OK);
    assert(zeal_debug_vram(h, DBG_PALETTE, 0, encoded, 43) == DBG_CAPACITY);
    assert(zeal_debug_vram(h, DBG_PALETTE, 0, encoded, sizeof(encoded)) == DBG_OK);
    assert(encoded[20] == 16 && encoded[24] == 16);
    assert(zeal_debug_vram(h, DBG_PALETTE, 256, encoded, sizeof(encoded)) == DBG_RANGE);
    debugger_reset(d);
    assert(machine.cpu.pc == 0);
    assert(debugger_command(d, DBG_STOP) == DBG_OK && machine.should_exit);
    debugger_unbind(d);
    debugger_deinit(d);
    zvb_deinit(&machine.zvb);
    return 0;
}
