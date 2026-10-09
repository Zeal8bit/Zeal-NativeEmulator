/* SPDX-License-Identifier: Apache-2.0 */
#include "hw/zvb/zvb_rpu.h"
#include <string.h>

#define RPU_REGISTER_MASK                (15)
#define RPU_HPOS_MASK                    (0x3ff)
#define RPU_VPOS_MASK                    (0x1ff)
#define RPU_WAIT_HPOS_MASK               (127)
#define RPU_HPOS_SHIFT                   (3)
#define RPU_VPOS_SHIFT                   (7)
#define RPU_BYTE_MASK                    (255)
#define RPU_CTRL_START                   (0x80)
#define RPU_CTRL_RESET                   (0x40)
#define RPU_CTRL_INT_ENABLE              (4)
#define RPU_INT_PENDING                  (1)
#define RPU_OPCODE_WAIT                  (0x00)
#define RPU_OPCODE_JUMP                  (0x20)
#define RPU_OPCODE_SKIP                  (0x40)
#define RPU_OPCODE_MASK                  (0x60)
#define RPU_OPCODE_SHIFT                 (16)
#define RPU_LOAD_CLASS_SHIFT             (22)
#define RPU_LOAD_CLASS                   (2)
#define RPU_LOAD_ADDR_MASK               (0x3fff)
#define RPU_UPLOAD_HIGH_MASK             (0xff00)

enum { IDLE, FETCH, EXECUTE };

void zvb_rpu_reset(zvb_rpu_t* rpu)
{
    /* Board reset retains program RAM; CTRL reset also retains the masks. */
    rpu->instruction = 0;
    rpu->upload_data = 0;
    rpu->pc = 0;
    rpu->upload_count = 0;
    rpu->upload_addr = 0;
    rpu->state = IDLE;
    rpu->hmask = RPU_HPOS_MASK;
    rpu->vmask = RPU_VPOS_MASK;
    rpu->int_raw = false;
    rpu->int_enabled = false;
}

void zvb_rpu_init(zvb_rpu_t* rpu)
{
    memset(rpu, 0, sizeof(*rpu));
    zvb_rpu_reset(rpu);
}

uint8_t zvb_rpu_read(const zvb_rpu_t* rpu, uint32_t address)
{
    switch (address & RPU_REGISTER_MASK) {
        case RPU_REG_CTRL:
            return (rpu->state == IDLE) | (rpu->int_raw << 1) | (rpu->int_enabled << 2);
        case RPU_REG_UPLOAD_ADDR:
            return rpu->upload_addr;
        default:
            return 0;
    }
}

void zvb_rpu_write(zvb_rpu_t* rpu, uint32_t address, uint8_t data)
{
    switch (address & RPU_REGISTER_MASK) {
        case RPU_REG_CTRL:
            rpu->state = (data & RPU_CTRL_START) ? FETCH : IDLE;
            rpu->int_enabled = (data & RPU_CTRL_INT_ENABLE) != 0;
            if (data & RPU_CTRL_RESET) {
                rpu->state = IDLE;
                rpu->upload_count = 0;
                rpu->upload_addr = 0;
                rpu->pc = 0;
                rpu->upload_data = 0;
                rpu->int_raw = false;
                rpu->int_enabled = false;
            }
            break;
        case RPU_REG_UPLOAD_ADDR:
            rpu->upload_addr = data;
            rpu->upload_count = 0;
            break;
        case RPU_REG_UPLOAD_DATA:
            if (rpu->upload_count == 0) {
                rpu->upload_data = (rpu->upload_data & RPU_UPLOAD_HIGH_MASK) | data;
                rpu->upload_count = 1;
            } else if (rpu->upload_count == 1) {
                rpu->upload_data = (data << 8) | (rpu->upload_data & RPU_BYTE_MASK);
                rpu->upload_count = 2;
            } else {
                rpu->program[rpu->upload_addr++] = ((uint32_t)data << 16) | rpu->upload_data;
                rpu->upload_count = 0;
            }
            break;
        case RPU_REG_INT_STATUS:
            rpu->int_raw = (data & RPU_INT_PENDING) != 0;
            break;
        default:
            break;
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
        const uint16_t iv = (instruction >> RPU_VPOS_SHIFT) & RPU_VPOS_MASK;
        const uint16_t ih = (instruction & RPU_WAIT_HPOS_MASK) << RPU_HPOS_SHIFT;
        const uint16_t v = vpos & rpu->vmask;
        const uint16_t h = hpos & rpu->hmask;
        const bool match = v > (iv & rpu->vmask) ||
            (v == (iv & rpu->vmask) && h >= (ih & rpu->hmask));
        rpu->state = FETCH;
        switch (instruction >> RPU_OPCODE_SHIFT) {
            case RPU_OPCODE_WAIT:
                if (match) {
                    rpu->pc++;
                } else {
                    rpu->state = EXECUTE;
                }
                break;
            case RPU_OPCODE_JUMP:
                rpu->pc = instruction & RPU_BYTE_MASK;
                break;
            case RPU_OPCODE_SKIP:
                rpu->pc += match ? 2 : 1;
                break;
            case RPU_OPCODE_MASK:
                rpu->vmask = iv;
                rpu->hmask = ih;
                rpu->pc++;
                break;
            default:
                rpu->pc++;
                break;
        }
    }
    /* Frame restart has priority over instruction execution. */
    if (state != IDLE && hpos == 0 && (vpos & RPU_VPOS_MASK) == 0) {
        rpu->pc = 0;
        rpu->state = FETCH;
    }
    /* LOAD is combinatorial on the old EXECUTE state. Self-writes take priority. */
    if (state == EXECUTE && (instruction >> RPU_LOAD_CLASS_SHIFT) == RPU_LOAD_CLASS) {
        load(userdata, (instruction >> 8) & RPU_LOAD_ADDR_MASK, instruction & RPU_BYTE_MASK);
    }
}
