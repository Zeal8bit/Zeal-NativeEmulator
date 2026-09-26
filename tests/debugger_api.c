#include "hw/debugger/bindings_internal.h"
#include "hw/debugger/debugger_internal.h"
#include <assert.h>
#include <string.h>
static bool paused;
static uint8_t ram[65536];
static void pause_cb(dbg_t *d)
{
    (void)d;
    paused = true;
}
static void continue_cb(dbg_t *d)
{
    (void)d;
    paused = false;
}
static bool paused_cb(dbg_t *d)
{
    (void)d;
    return paused;
}
static void stop_cb(dbg_t *d) { (void)d; }
static int read_cb(dbg_t *d, hwaddr a, int n, uint8_t *p)
{
    (void)d;
    memcpy(p, ram + a, n);
    return 0;
}
static int write_cb(dbg_t *d, hwaddr a, int n, uint8_t *p)
{
    (void)d;
    memcpy(ram + a, p, n);
    return 0;
}
int main(void)
{
    static dbg_t d;
    debugger_init(&d);
    d.pause_cb = pause_cb;
    d.continue_cb = continue_cb;
    d.stop_cb = stop_cb;
    d.is_paused_cb = paused_cb;
    d.get_mem_cb = read_cb;
    d.set_mem_cb = write_cb;
    assert(!debugger_is_watchpoint_set(&d, 0));
    assert(debugger_add_watchpoint(&d, (watchpoint_t){0, WATCHPOINT_READ}));
    assert(debugger_add_watchpoint(&d, (watchpoint_t){1, WATCHPOINT_WRITE}));
    watchpoint_t wp[2];
    assert(debugger_get_watchpoints(&d, wp, 2) == 2);
    assert(debugger_remove_watchpoint(&d, 0));
    assert(!debugger_is_watchpoint_set(&d, 0));
    assert(debugger_set_breakpoint(&d, 0));
    assert(debugger_is_breakpoint_set(&d, 0));
    assert(debugger_clear_breakpoint(&d, 0));
    assert(debugger_command(NULL, DBG_PAUSE) == DBG_INVALID);
    assert(debugger_command(&d, DBG_PAUSE) == DBG_OK && paused);
    assert(debugger_command(&d, DBG_CONTINUE) == DBG_OK && !paused);
    uint8_t b = 42, out = 0;
    assert(debugger_memory_write(&d, DBG_VIRTUAL, 65535, &b, 1) == DBG_OK);
    assert(debugger_memory_read(&d, DBG_VIRTUAL, 65535, &out, 1) == DBG_OK && out == 42);
    assert(debugger_memory_read(&d, DBG_VIRTUAL, 65535, &out, 2) == DBG_RANGE);
    assert(debugger_memory_read(&d, DBG_VIRTUAL, 0, NULL, 1) == DBG_INVALID);
    const char text[] = "hello = $1234 ; addr, public\n";
    for (int i = 0; i < 70; i++)
        assert(debugger_symbols_text(&d, text, sizeof(text) - 1) == DBG_OK);
    hwaddr a;
    assert(debugger_find_symbol(&d, "hello", &a) && a == 0x1234);
    for (int i = 0; i < 70; i++)
        debugger_record(&d, DBG_REASON_STEP, i, 0);
    dbg_event_record_t records[64];
    uint32_t count, overflow;
    assert(debugger_events(&d, 0, records, 64, &count, &overflow) == DBG_OK && count == 64 && overflow);
    assert(records[63].sequence == 72 && records[63].address == 69);
    assert(debugger_events(&d, 72, records, 64, &count, &overflow) == DBG_OK && count == 0 && !overflow);
    uint32_t handle = debugger_bind(&d);
    uint8_t encoded[64] = {0};
    assert(zeal_debug_handle() == handle);
    assert(zeal_debug_state(handle, encoded, 23) == DBG_CAPACITY);
    assert(zeal_debug_state(handle, encoded, 24) == DBG_OK && encoded[16] == 72);
    assert(zeal_debug_memory(handle, 0, 0, 65535, encoded, 1) == DBG_OK && encoded[0] == 42);
    assert(zeal_debug_breakpoint(handle, 0x1234, 1) == DBG_OK);
    assert(zeal_debug_points(handle, 0, encoded, sizeof(encoded)) == DBG_OK && encoded[0] == 1 &&
           encoded[4] == 0x34 && encoded[5] == 0x12);
    debugger_unbind(&d);
    assert(zeal_debug_state(handle, encoded, 24) == DBG_INVALID);
    assert(debugger_bind(&d) != handle);
    debugger_unbind(&d);
    assert(debugger_command(&d, DBG_STOP) == DBG_OK);
    dbg_snapshot_t snap;
    assert(debugger_snapshot(&d, &snap) == DBG_OK && snap.stopped);
    assert(debugger_command(&d, DBG_CONTINUE) == DBG_UNAVAILABLE);
    debugger_deinit(&d);
    assert(!debugger_get_symbol(&d, 0x1234));
    return 0;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static dbg_t browser_fixture;
EMSCRIPTEN_KEEPALIVE uint32_t test_debugger_create(void)
{
    debugger_init(&browser_fixture);
    browser_fixture.pause_cb = pause_cb;
    browser_fixture.continue_cb = continue_cb;
    browser_fixture.stop_cb = stop_cb;
    browser_fixture.is_paused_cb = paused_cb;
    browser_fixture.get_mem_cb = read_cb;
    browser_fixture.set_mem_cb = write_cb;
    return debugger_bind(&browser_fixture);
}
EMSCRIPTEN_KEEPALIVE void test_debugger_destroy(void)
{
    debugger_unbind(&browser_fixture);
    debugger_deinit(&browser_fixture);
}
#endif
