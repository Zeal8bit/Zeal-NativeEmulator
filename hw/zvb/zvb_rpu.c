/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zvb/zvb_rpu.h"
#include <string.h>

enum { IDLE, FETCH, EXECUTE };

void zvb_rpu_reset(zvb_rpu_t* rpu)
{
    /* FPGA reset retains program RAM; CTRL reset also retains the masks. */
    rpu->instruction = 0;
    rpu->upload_data = 0;
    rpu->upload_addr = rpu->upload_count = rpu->pc = 0;
    rpu->state = IDLE;
    rpu->hmask = 0x3ff;
    rpu->vmask = 0x1ff;
    rpu->int_enabled = rpu->int_raw = false;
}

void zvb_rpu_init(zvb_rpu_t* rpu)
{
    memset(rpu, 0, sizeof(*rpu));
    zvb_rpu_reset(rpu);
}

uint8_t zvb_rpu_read(const zvb_rpu_t* rpu, uint32_t address)
{
    switch (address & 15) {
        case 0: return (rpu->state == IDLE) | (rpu->int_raw << 1) | (rpu->int_enabled << 2);
        case 1: return rpu->upload_addr;
        default: return 0;
    }
}

void zvb_rpu_write(zvb_rpu_t* rpu, uint32_t address, uint8_t data)
{
    switch (address & 15) {
        case 0:
            rpu->state = (data & 0x80) ? FETCH : IDLE;
            rpu->int_enabled = (data & 4) != 0;
            if (data & 0x40) {
                rpu->state = IDLE;
                rpu->pc = rpu->upload_addr = rpu->upload_count = 0;
                rpu->upload_data = 0;
                rpu->int_enabled = rpu->int_raw = false;
            }
            break;
        case 1:
            rpu->upload_addr = data;
            rpu->upload_count = 0;
            break;
        case 2:
            if (rpu->upload_count == 0) {
                rpu->upload_data = (rpu->upload_data & 0xff00) | data;
                rpu->upload_count = 1;
            } else if (rpu->upload_count == 1) {
                rpu->upload_data = (data << 8) | (rpu->upload_data & 255);
                rpu->upload_count = 2;
            } else {
                rpu->program[rpu->upload_addr++] = ((uint32_t)data << 16) | rpu->upload_data;
                rpu->upload_count = 0;
            }
            break;
        case 3: rpu->int_raw = (data & 1) != 0; break;
        default: break;
    }
}

void zvb_rpu_clock(zvb_rpu_t* rpu, uint16_t hpos, uint16_t vpos,
                   zvb_rpu_load_fn load, void* userdata)
{
    const uint8_t state = rpu->state;
    const uint32_t instruction = rpu->instruction;
    if (state == FETCH) {
        rpu->instruction = rpu->program[rpu->pc];
        rpu->state = EXECUTE;
    } else if (state == EXECUTE) {
        const uint16_t iv = (instruction >> 7) & 0x1ff;
        const uint16_t ih = (instruction & 127) << 3;
        const uint16_t v = vpos & rpu->vmask;
        const uint16_t h = hpos & rpu->hmask;
        const bool match = v > (iv & rpu->vmask) ||
            (v == (iv & rpu->vmask) && h >= (ih & rpu->hmask));
        rpu->state = FETCH;
        switch (instruction >> 16) {
            case 0x00:
                if (match) rpu->pc++;
                else rpu->state = EXECUTE;
                break;
            case 0x20: rpu->pc = instruction & 255; break;
            case 0x40: rpu->pc += match ? 2 : 1; break;
            case 0x60:
                rpu->vmask = iv;
                rpu->hmask = ih;
                rpu->pc++;
                break;
            default: rpu->pc++; break;
        }
    }
    /* Frame restart has priority over instruction execution. */
    if (state != IDLE && hpos == 0 && (vpos & 0x1ff) == 0) {
        rpu->pc = 0;
        rpu->state = FETCH;
    }
    /* LOAD is combinatorial on the old EXECUTE state. Self-writes take priority. */
    if (state == EXECUTE && (instruction >> 22) == 2)
        load(userdata, (instruction >> 8) & 0x3fff, instruction & 255);
}
