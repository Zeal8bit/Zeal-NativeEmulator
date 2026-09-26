/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "debugger.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* All calls run on the emulation thread between instruction slices. Contexts
 * belong to the host. Buffers belong to the caller; no API frees them. */
typedef enum { DBG_OK, DBG_INVALID, DBG_RANGE, DBG_CAPACITY, DBG_UNAVAILABLE } dbg_status_t;
typedef enum { DBG_VIRTUAL, DBG_PHYSICAL } dbg_address_space_t;
typedef enum { DBG_PAUSE, DBG_CONTINUE, DBG_STEP, DBG_STEP_OVER, DBG_RESET, DBG_STOP } dbg_command_t;
typedef enum { DBG_REASON_NONE, DBG_REASON_PAUSE, DBG_REASON_BREAKPOINT,
    DBG_REASON_WATCHPOINT, DBG_REASON_STEP, DBG_REASON_RESET, DBG_REASON_STOP } dbg_reason_t;
typedef struct { uint32_t paused, stopped, reason, pc; uint64_t sequence; } dbg_snapshot_t;
typedef struct { uint64_t sequence; uint32_t reason, address, access; } dbg_event_record_t;
typedef struct { uint32_t virtual_address, physical_address, page; char device[64]; } dbg_mapping_t;
typedef struct {
    uint64_t start, last_split, minimum_us, maximum_us, total_us, last_us;
    uint32_t samples, running, break_on_update;
} dbg_counter_t;
typedef struct { uint32_t width, height, stride; uint64_t generation; } dbg_image_info_t;

dbg_status_t debugger_command(dbg_t*, dbg_command_t);
dbg_status_t debugger_snapshot(dbg_t*, dbg_snapshot_t*);
/* sequence is the last observed event (initially zero). Overflow indicates
 * overwritten records; returned records are oldest first. Capacity zero queries
 * available count without consuming anything. */
dbg_status_t debugger_events(dbg_t*, uint64_t after, dbg_event_record_t*, uint32_t capacity,
                             uint32_t* count, uint32_t* overflow);
/* No wrapping. Virtual addresses 0..65535; physical addresses 0..4194303.
 * Writes use device semantics (ROM may ignore them); reads may have device side effects. */
dbg_status_t debugger_memory_read(dbg_t*, dbg_address_space_t, uint32_t, uint8_t*, uint32_t);
dbg_status_t debugger_memory_write(dbg_t*, dbg_address_space_t, uint32_t, const uint8_t*, uint32_t);
dbg_status_t debugger_symbols_text(dbg_t*, const char*, uint32_t);
dbg_status_t debugger_mappings(dbg_t*, dbg_mapping_t out[4]);
dbg_status_t debugger_counters(dbg_t*, dbg_counter_t out[8]);
dbg_status_t debugger_counter_break(dbg_t*, uint32_t index, uint32_t enabled);
/* Rendered image APIs require initialized graphics; core inspection does not.
 * view=-1 means video. Query with NULL/zero, then provide width*height*4 bytes.
 * Pixels are top-down RGBA8. Copy stays valid until caller changes its buffer. */
dbg_status_t debugger_image_copy(dbg_t*, int32_t view, dbg_image_info_t*, uint8_t*, uint32_t capacity);

#ifdef __cplusplus
}
#endif
