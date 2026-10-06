/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* ZealRasterProcessingUnit.v: 256 little-endian, 24-bit instructions. */
typedef struct {
    uint32_t program[256];
    uint32_t instruction;
    uint16_t upload_data, hmask, vmask;
    uint8_t upload_addr, upload_count, pc, state;
    bool int_enabled, int_raw;
} zvb_rpu_t;

typedef void (*zvb_rpu_load_fn)(void* userdata, uint16_t address, uint8_t data);
void zvb_rpu_init(zvb_rpu_t* rpu);
void zvb_rpu_reset(zvb_rpu_t* rpu);
uint8_t zvb_rpu_read(const zvb_rpu_t* rpu, uint32_t address);
void zvb_rpu_write(zvb_rpu_t* rpu, uint32_t address, uint8_t data);
/* One 50 MHz FPGA clock. LOAD uses the board's existing memory decoder. */
void zvb_rpu_clock(zvb_rpu_t* rpu, uint16_t hpos, uint16_t vpos,
                   zvb_rpu_load_fn load, void* userdata);
static inline bool zvb_rpu_active(const zvb_rpu_t* rpu) { return rpu->state != 0; }
static inline bool zvb_rpu_interrupt(const zvb_rpu_t* rpu)
{ return rpu->int_enabled && rpu->int_raw; }
