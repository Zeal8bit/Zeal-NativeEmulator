/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware/io_registers_view.h"
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include "common/machine.h"
#include "frontend/sdl-fltk/frontend.h"
#include "hardware/hw_register_view.h"
#include "tools/tool_registry.h"
#include "widgets/panel.h"

namespace zui::hardware {


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

static std::vector<HwRegisterView::Chip> bits(uint8_t value, const char* const* names)
{
    std::vector<HwRegisterView::Chip> out;
    for (int b = 7; b >= 0; b--) out.push_back({ names[b], ((value >> b) & 1) != 0 });
    return out;
}

static std::string hostfs_status(unsigned st)
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

static const char* i2c_device(uint8_t addr)
{
    const unsigned a7 = addr >> 1;
    if (addr == 0x68 || a7 == 0x68) {
        return "DS1307";
    }
    if (addr == 0x50 || a7 == 0x50) {
        return "AT24C512";
    }
    return "";
}

static widgets::Panel* make_hw_io(int x, int y, int w, int h)
{
    return new IoRegistersView(x, y, w, h);
}

static tools::ToolViewRegistrar s_reg(tools::TOOL_HARDWARE, 30, "I/O", make_hw_io);

IoRegistersView::IoRegistersView(int X, int Y, int W, int H)
    : HwRegisterView(X, Y, W, H, "I/O DEVICES")
{}

static int pin_bit(const port_t* port, int pin)
{
    return (port->state >> pin) & 1;
}

static unsigned bcd(uint8_t v)
{
    return (unsigned) ((v >> 4) * 10 + (v & 0xF));
}

void IoRegistersView::build()
{
    if (fltk_machine == nullptr) {
        section("NO MACHINE");
        return;
    }
    const zeal_t* m = fltk_machine;
    const port_t* port_b = &m->pio.port_b;

    const port_t* ports[2] = { &m->pio.port_a, &m->pio.port_b };
    for (int p = 0; p < 2; p++) {
        const port_t* port = ports[p];
        const char* const* pins = p ? s_port_b_pins : s_port_a_pins;
        section(p ? "PIO PORT B  (SYSTEM)" : "PIO PORT A  (USER)");
        u8("DATA", port->state);
        chips("PINS", bits(port->state, pins));
        u8("MODE", port->mode, name_of(s_pio_modes, 4, port->mode));
        u8("DIRECTION", port->dir, port->mode == 3 ? "1 = input" : "bit-ctrl only");
        u8("INT VECTOR", port->int_vector);
        u8("INT MASK", port->int_mask);
        chips("INT CTRL", { { "EN", port->int_enable != 0 }, { "AND", port->and_op != 0 },
                            { "HIGH", port->active_high != 0 } })
            .tip = "Interrupt enable, AND (all) vs OR (any) pins, active high";
    }

    uint8_t tx_bits = 0;
    for (int i = 1; i < m->uart.tx_pos && i < 9; i++) {
        tx_bits |= (uint8_t) ((m->uart.tx_fifo[i] & 1) << (i - 1));
    }
    section("UART");
    text("BAUD", "57600 8N1  (bit-banged on port B)");
    chips("LINES", { { "TX", pin_bit(port_b, IO_UART_TX_PIN) != 0 },
                     { "RX", pin_bit(port_b, IO_UART_RX_PIN) != 0 } });
    u8("TX BIT", m->uart.tx_pos, "of 10");
    u8("TX SHIFT", tx_bits, "partial byte");

    section("I2C BUS");
    text("STATE", name_of(s_i2c_states, 5, (int) m->i2c_bus.st));
    u8("DEVICE", m->i2c_bus.dev_addr, i2c_device(m->i2c_bus.dev_addr));
    u8("SHIFT BYTE", m->i2c_bus.cur_byte);
    u8("BIT", (unsigned) m->i2c_bus.cur_bit);
    chips("LINES", { { "SCL", pin_bit(port_b, IO_I2C_SCL_OUT_PIN) != 0 },
                     { "SDA", pin_bit(port_b, IO_I2C_SDA_OUT_PIN) != 0 },
                     { "SDI", pin_bit(port_b, IO_I2C_SDA_IN_PIN) != 0 },
                     { "REPLY", m->i2c_bus.has_reply != 0 } });

    const ds1307_t& rtc = m->rtc;
    int year, month, day, wday, hour, minute, second;
    const time_t now = time(NULL) + (time_t) rtc.time_diff;
    const struct tm* tm = localtime(&now);
    if (tm != nullptr) {
        year = tm->tm_year + 1900;
        month = tm->tm_mon + 1;
        day = tm->tm_mday;
        wday = tm->tm_wday;
        hour = tm->tm_hour;
        minute = tm->tm_min;
        second = tm->tm_sec;
    } else {
        year = 2000 + (int) bcd(rtc.ram[6]);
        month = (int) bcd(rtc.ram[5] & 0x1F);
        day = (int) bcd(rtc.ram[4] & 0x3F);
        wday = (int) bcd(rtc.ram[3] & 7) - 1;
        hour = (int) bcd(rtc.ram[2] & 0x3F);
        minute = (int) bcd(rtc.ram[1] & 0x7F);
        second = (int) bcd(rtc.ram[0] & 0x7F);
    }
    section("RTC  DS1307  ($68)");
    text("DATE", fmt("%04d-%02d-%02d %s", year, month, day,
                     wday >= 0 && wday < 7 ? s_weekdays[wday] : "?"));
    text("TIME", fmt("%02d:%02d:%02d", hour, minute, second));
    text("OFFSET", fmt("%+lld s from host", (long long) rtc.time_diff));
    u8("REGISTER", rtc.reg);
    u8("CONTROL", rtc.ram[7], (rtc.ram[7] & 0x10) ? "SQW on" : "SQW off");
    chips("CLOCK", { { "HALT", (rtc.ram[0] & 0x80) != 0 } });

    const at24c512_t& eeprom = m->eeprom;
    section("EEPROM  AT24C512  ($50)");
    u16("ADDRESS", eeprom.address);
    u8("BYTE COUNT", (unsigned) eeprom.count);
    chips("STATE", { { "WRITING", eeprom.writing } });
    u16("WRITE PAGE", (unsigned) eeprom.sector_written, eeprom.writing ? "128-byte page" : "last");

    section("PS/2 KEYBOARD");
    text("STATE", name_of(s_ps2_states, 3, (int) m->keyboard.state));
    u8("SHIFT REG", m->keyboard.shift_register);
    u16("FIFO", (unsigned) fifo_size((fifo_t*) &m->keyboard.queue), "of 512");
    chips("LINE", { { "PIN HIGH", m->keyboard.pin_state != 0 } });

    const compactflash_t* cf = &m->compactflash;
    section("COMPACTFLASH  ($70)");
    if (m->cpu.mmu.io_mapping[0x70].dev != &cf->parent) {
        text("CARD", "not attached");
    } else {
        const uint32_t lba = (uint32_t) cf->lba_0 | ((uint32_t) cf->lba_8 << 8) |
                             ((uint32_t) cf->lba_16 << 16) | ((uint32_t) (cf->lba_24 & 0x0F) << 24);
        u8("STATUS", cf->status);
        chips("FLAGS", bits(cf->status, s_cf_status));
        u8("ERROR", cf->error);
        u8("FEATURE", cf->feature);
        text("STATE", name_of(s_cf_states, 5, (int) cf->state));
        row("LBA", hexn(lba, 7), dec(lba));
        text("SECTOR", fmt("%d of %d", cf->sec_cur, cf->sec_cnt));
        u16("BUFFER IDX", (unsigned) cf->sector_buffer_idx);
        text("CAPACITY", fmt("%llu sectors (%llu MB)", (unsigned long long) cf->total_sectors,
                             (unsigned long long) (cf->total_sectors / 2048)));
        chips("MODE", { { "LBA", cf->lba_mode != 0 }, { "MASTER", cf->master != 0 },
                        { "FAILED", cf->failed != 0 } });
    }

    const unsigned st = m->hostfs.registers[15];
    section("HOSTFS  ($C0)");
    u8("STATUS", st, hostfs_status(st));
    text("REG 0-3", h8(m->hostfs.registers[0]) + " " + h8(m->hostfs.registers[1]) + " " +
                        h8(m->hostfs.registers[2]) + " " + h8(m->hostfs.registers[3]));
    text("REG 4-7", h8(m->hostfs.registers[4]) + " " + h8(m->hostfs.registers[5]) + " " +
                        h8(m->hostfs.registers[6]) + " " + h8(m->hostfs.registers[7]));
    addr16("BUFFER", (unsigned) (m->hostfs.registers[1] | (m->hostfs.registers[2] << 8)), "regs 1-2")
        .tip = "Path/buffer pointer used by most operations";
    chips("BACKEND", { { "BOUND", m->hostfs.dispatch != NULL } });

    section("MMU  ($F0)");
    for (int i = 0; i < 4; i++) {
        const device_t* dev = m->cpu.mmu.vpages[i].dev;
        Item& it = u8(s_mmu_pages[i], m->cpu.mmu.pages[i],
                      fmt("%s %s", hexn((uint32_t) m->cpu.mmu.pages[i] << 14, 6).c_str(),
                          dev ? dev->name : "--"));
        it.has_addr = true;
        it.addr = (hwaddr) (i << 14);
        it.tip = fmt("Virtual %s-%s", h16((unsigned) i << 14).c_str(), h16(((unsigned) i << 14) | 0x3FFF).c_str());
    }
}

} // namespace zui::hardware
