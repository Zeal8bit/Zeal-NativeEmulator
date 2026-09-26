/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Integer handles, not serialized native pointers. Buffer layouts are little
 * endian and defined in ui/fltk/README.md. Allocate buffers with malloc/free
 * (the WASM module exports these); do not retain a pointer across memory growth. */
uint32_t zeal_debug_handle(void);
uint32_t zeal_debug_command(uint32_t handle, uint32_t command);
uint32_t zeal_debug_state(uint32_t handle, uint8_t *output, uint32_t capacity);
uint32_t zeal_debug_events(uint32_t handle, uint32_t after_low, uint32_t after_high, uint8_t *output,
                           uint32_t capacity);
uint32_t zeal_debug_memory(uint32_t handle, uint32_t write, uint32_t physical, uint32_t address,
                           uint8_t *bytes, uint32_t count);
uint32_t zeal_debug_registers(uint32_t handle, uint32_t write, uint8_t *bytes, uint32_t capacity);
uint32_t zeal_debug_breakpoint(uint32_t handle, uint32_t address, uint32_t enabled);
uint32_t zeal_debug_watchpoint(uint32_t handle, uint32_t address, uint32_t access);
uint32_t zeal_debug_points(uint32_t handle, uint32_t watchpoints, uint8_t *output, uint32_t capacity);
uint32_t zeal_debug_symbols(uint32_t handle, const char *text, uint32_t length);
uint32_t zeal_debug_disassemble(uint32_t handle, uint32_t address, uint8_t *output, uint32_t capacity);
uint32_t zeal_debug_mappings(uint32_t handle, uint8_t *output, uint32_t capacity);
uint32_t zeal_debug_counters(uint32_t handle, uint8_t *output, uint32_t capacity);
uint32_t zeal_debug_counter_break(uint32_t handle, uint32_t index, uint32_t enabled);
uint32_t zeal_debug_vram(uint32_t handle, int32_t view, uint32_t index, uint8_t *output, uint32_t capacity);
uint32_t zeal_debug_image(uint32_t handle, int32_t view, uint8_t *info, uint32_t info_capacity,
                          uint8_t *pixels, uint32_t pixel_capacity);
#ifdef __cplusplus
}
#endif
