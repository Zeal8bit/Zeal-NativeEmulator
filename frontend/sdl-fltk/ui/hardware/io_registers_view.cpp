/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware/io_registers_view.h"
#include <cstdint>
#include <string>
#include <vector>
#include "common/machine.h"
#include "frontend/sdl-fltk/bridge/hw.h"
#include "hardware/hw_register_view.h"
#include "tools/tool_registry.h"
#include "widgets/panel.h"

namespace zui::hardware {

using namespace zui::tools;
using namespace zui::widgets;

namespace {

const char* const s_pio_modes[] = { "OUTPUT", "INPUT", "BIDIR", "BIT CTRL" };
/* System port (B) pin functions, bit 0..7 */
const char* const s_port_b_pins[] = { "SDA", "SCL", "SDI", "RX", "TX", "GP", "VBL", "KBD" };
const char* const s_port_a_pins[] = { "0", "1", "2", "3", "4", "5", "6", "7" };
const char* const s_i2c_states[] = { "IDLE", "START", "ADDRESS", "RESTART", "READDRESS" };
const char* const s_ps2_states[] = { "IDLE", "ACTIVE", "INACTIVE" };
const char* const s_cf_states[] = { "IDLE", "COMMAND", "DATA IN", "DATA OUT", "ERROR" };
const char* const s_cf_status[] = { "ERR", "IDX", "CORR", "DRQ", "DSC", "DWF", "RDY", "BSY" };
const char* const s_weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char* const s_mmu_pages[] = { "PAGE 0", "PAGE 1", "PAGE 2", "PAGE 3" };

std::vector<HwRegisterView::Chip> bits(uint8_t value, const char* const* names)
{
    std::vector<HwRegisterView::Chip> out;
    for (int b = 7; b >= 0; b--) out.push_back({ names[b], ((value >> b) & 1) != 0 });
    return out;
}

std::string hostfs_status(unsigned st)
{
    switch (st) {
        case 0: return "SUCCESS";
        case 1: return "FAILURE";
        case 4: return "NO SUCH ENTRY";
        case 20: return "CANNOT REGISTER";
        case 21: return "NO MORE ENTRIES";
        case 0xFF: return "PENDING";
        default: return "error " + std::to_string(st);
    }
}

const char* i2c_device(uint8_t addr)
{
    const unsigned a7 = addr >> 1;
    if (addr == 0x68 || a7 == 0x68) return "DS1307";
    if (addr == 0x50 || a7 == 0x50) return "AT24C512";
    return "";
}

widgets::Panel* make_hw_io(int x, int y, int w, int h)
{
    return new IoRegistersView(x, y, w, h);
}

tools::ToolViewRegistrar s_reg(tools::TOOL_HARDWARE, 30, "I/O", make_hw_io);

} // namespace

IoRegistersView::IoRegistersView(int X, int Y, int W, int H)
    : HwRegisterView(X, Y, W, H, "I/O DEVICES")
{}

void IoRegistersView::build()
{
    fz_hw_io_t io;
    if (!fz_hw_io(&io)) {
        section("NO MACHINE");
        return;
    }

    for (int p = 0; p < 2; p++) {
        const auto& port = io.pio[p];
        const char* const* pins = p ? s_port_b_pins : s_port_a_pins;
        section(p ? "PIO PORT B  (SYSTEM)" : "PIO PORT A  (USER)");
        u8("DATA", port.state);
        chips("PINS", bits(port.state, pins));
        u8("MODE", port.mode, name_of(s_pio_modes, 4, port.mode));
        u8("DIRECTION", port.dir, port.mode == 3 ? "1 = input" : "bit-ctrl only");
        u8("INT VECTOR", port.int_vector);
        u8("INT MASK", port.int_mask);
        chips("INT CTRL", { { "EN", port.int_enable }, { "AND", port.and_op }, { "HIGH", port.active_high } })
            .tip = "Interrupt enable, AND (all) vs OR (any) pins, active high";
    }

    section("UART");
    text("BAUD", "57600 8N1  (bit-banged on port B)");
    chips("LINES", { { "TX", io.uart.tx_pin }, { "RX", io.uart.rx_pin } });
    u8("TX BIT", io.uart.tx_pos, "of 10");
    u8("TX SHIFT", io.uart.tx_bits, "partial byte");

    section("I2C BUS");
    text("STATE", name_of(s_i2c_states, 5, io.i2c.state));
    u8("DEVICE", io.i2c.dev_addr, i2c_device(io.i2c.dev_addr));
    u8("SHIFT BYTE", io.i2c.cur_byte);
    u8("BIT", (unsigned) io.i2c.cur_bit);
    chips("LINES", { { "SCL", io.i2c.scl }, { "SDA", io.i2c.sda_out }, { "SDI", io.i2c.sda_in },
                     { "REPLY", io.i2c.has_reply } });

    section("RTC  DS1307  ($68)");
    text("DATE", fmt("%04d-%02d-%02d %s", io.rtc.year, io.rtc.month, io.rtc.day,
                     io.rtc.wday >= 0 && io.rtc.wday < 7 ? s_weekdays[io.rtc.wday] : "?"));
    text("TIME", fmt("%02d:%02d:%02d", io.rtc.hour, io.rtc.minute, io.rtc.second));
    text("OFFSET", fmt("%+lld s from host", (long long) io.rtc.offset_s));
    u8("REGISTER", io.rtc.reg);
    u8("CONTROL", io.rtc.ram[7], (io.rtc.ram[7] & 0x10) ? "SQW on" : "SQW off");
    chips("CLOCK", { { "HALT", io.rtc.halted } });

    section("EEPROM  AT24C512  ($50)");
    u16("ADDRESS", io.eeprom.address);
    u8("BYTE COUNT", (unsigned) io.eeprom.count);
    chips("STATE", { { "WRITING", io.eeprom.writing } });
    u16("WRITE PAGE", (unsigned) io.eeprom.sector_written, io.eeprom.writing ? "128-byte page" : "last");

    section("PS/2 KEYBOARD");
    text("STATE", name_of(s_ps2_states, 3, io.keyboard.state));
    u8("SHIFT REG", io.keyboard.shift_register);
    u16("FIFO", (unsigned) io.keyboard.fifo_count, "of 512");
    chips("LINE", { { "PIN HIGH", io.keyboard.pin } });

    section("COMPACTFLASH  ($70)");
    if (!io.cf.present) {
        text("CARD", "not attached");
    } else {
        u8("STATUS", io.cf.status);
        chips("FLAGS", bits(io.cf.status, s_cf_status));
        u8("ERROR", io.cf.error);
        u8("FEATURE", io.cf.feature);
        text("STATE", name_of(s_cf_states, 5, io.cf.state));
        row("LBA", hexn(io.cf.lba, 7), dec(io.cf.lba));
        text("SECTOR", fmt("%d of %d", io.cf.sec_cur, io.cf.sec_cnt));
        u16("BUFFER IDX", (unsigned) io.cf.buffer_idx);
        text("CAPACITY", fmt("%llu sectors (%llu MB)", (unsigned long long) io.cf.total_sectors,
                             (unsigned long long) (io.cf.total_sectors / 2048)));
        chips("MODE", { { "LBA", io.cf.lba_mode }, { "MASTER", io.cf.master }, { "FAILED", io.cf.failed } });
    }

    section("HOSTFS  ($C0)");
    const unsigned st = io.hostfs.regs[15];
    u8("STATUS", st, hostfs_status(st));
    text("REG 0-3", h8(io.hostfs.regs[0]) + " " + h8(io.hostfs.regs[1]) + " " + h8(io.hostfs.regs[2]) + " " +
                        h8(io.hostfs.regs[3]));
    text("REG 4-7", h8(io.hostfs.regs[4]) + " " + h8(io.hostfs.regs[5]) + " " + h8(io.hostfs.regs[6]) + " " +
                        h8(io.hostfs.regs[7]));
    addr16("BUFFER", (unsigned) (io.hostfs.regs[1] | (io.hostfs.regs[2] << 8)), "regs 1-2")
        .tip = "Path/buffer pointer used by most operations";
    chips("BACKEND", { { "BOUND", io.hostfs.bound } });

    section("MMU  ($F0)");
    for (int i = 0; i < 4; i++) {
        Item& it = u8(s_mmu_pages[i], io.mmu.pages[i],
                      fmt("%s %s", hexn((uint32_t) io.mmu.pages[i] << 14, 6).c_str(),
                          io.mmu.device[i] ? io.mmu.device[i] : "--"));
        it.has_addr = true;
        it.addr = (hwaddr) (i << 14);
        it.tip = fmt("Virtual %s-%s", h16((unsigned) i << 14).c_str(), h16(((unsigned) i << 14) | 0x3FFF).c_str());
    }
}

} // namespace zui::hardware
