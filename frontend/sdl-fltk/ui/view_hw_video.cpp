/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Hardware window: Zeal Video Board registers. */

#include "hw_view.h"

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

namespace {

struct ModeInfo {
    const char* name;
    const char* resolution;
    const char* colors;
};

const ModeInfo s_modes[] = {
    { "TEXT 640",     "640x480  80x40 chars", "16 (4-bit)" },
    { "TEXT 320",     "320x240  40x20 chars", "16 (4-bit)" },
    { "BITMAP 256",   "256x240",              "256 (8-bit)" },
    { "BITMAP 320",   "320x200",              "256 (8-bit)" },
    { "GFX 640 8BIT", "640x480  80x40 tiles", "256 (8-bit)" },
    { "GFX 320 8BIT", "320x240  20x15 tiles", "256 (8-bit)" },
    { "GFX 640 4BIT", "640x480  80x40 tiles", "16 (4-bit)" },
    { "GFX 320 4BIT", "320x240  20x15 tiles", "16 (4-bit)" },
};

const char* const s_banks[] = { "TEXT", "SPI", "CRC32", "SOUND", "DMA", "RPU", "TIMER" };
const char* const s_dma_states[] = { "IDLE", "REQUEST", "DESCRIPTOR", "READ", "WRITE", "RELEASE" };
const char* const s_rpu_states[] = { "IDLE", "FETCH", "EXECUTE" };
const char* const s_tf_states[] = { "WAIT IDLE", "IDLE", "CMD55", "READ BLOCK", "WRITE WAIT TOKEN",
                                    "WRITE BLOCK", "WRITE RESPONSE" };

} // namespace

class VideoRegistersView : public HwRegisterView {
public:
    VideoRegistersView(int X, int Y, int W, int H) : HwRegisterView(X, Y, W, H, "VIDEO BOARD") {}

protected:
    void build() override
    {
        fz_hw_video_t v;
        if (!fz_hw_video(&v)) {
            section("NO MACHINE");
            return;
        }
        const ModeInfo* mode = v.mode < 8 ? &s_modes[v.mode] : nullptr;

        section("VIDEO MODE");
        text("VERSION", fmt("v%u.%u.%u", v.major, v.minor, v.rev));
        u8("MODE", v.mode, mode ? mode->name : "?");
        text("RESOLUTION", mode ? mode->resolution : "?");
        text("COLORS", mode ? mode->colors : "?");
        u8("STATUS", v.status);
        chips("CONTROL", { { "VID", v.vid_ena }, { "HINT", v.h_int_ena }, { "VINT", v.v_int_ena },
                           { "OUT", v.screen_enabled } }).tip = "Video enable, h/v-blank interrupt enables, "
                                                               "output latched at vblank";
        u8("IO BANK", v.io_bank, name_of(s_banks, 7, v.io_bank));
        text("SCRATCH", h8(v.scratch[0]) + " " + h8(v.scratch[1]) + " " + h8(v.scratch[2]) + " " + h8(v.scratch[3]));

        section("RASTER");
        u16("SCANLINE", (unsigned) v.scanline);
        u16("HPOS", (unsigned) v.hpos);
        text("STATE", v.raster_state ? "HBLANK" : "RENDERING");
        chips("BLANK", { { "HBL", v.h_blank }, { "VBL", v.v_blank } });
        chips("LATCHED", { { "HBL", (v.blank_latches & 1) != 0 }, { "VBL", (v.blank_latches & 2) != 0 } });
        chips("IRQ", { { "GP", v.gp_irq }, { "SND", (v.ext_irq & 1) != 0 }, { "RPU", (v.ext_irq & 2) != 0 },
                       { "TMR", (v.ext_irq & 4) != 0 } });
        chips("RENDERER", { { "SCANLINE", v.scanline_rendering } });

        section("LAYERS");
        u16("L0 SCROLL X", v.l0_scroll_x);
        u16("L0 SCROLL Y", v.l0_scroll_y);
        u16("L1 SCROLL X", v.l1_scroll_x);
        u16("L1 SCROLL Y", v.l1_scroll_y);

        section("TEXT CONTROLLER");
        u8("CURSOR X", v.text.cursor_x);
        u8("CURSOR Y", v.text.cursor_y);
        text("SAVED", fmt("%u, %u", v.text.save_x, v.text.save_y));
        u8("SCROLL X", v.text.scroll_x);
        u8("SCROLL Y", v.text.scroll_y);
        u8("COLOR", v.text.color, fmt("fg %u bg %u", v.text.color & 0xF, v.text.color >> 4));
        u8("CURSOR COLOR", v.text.cursor_color,
           fmt("fg %u bg %u", v.text.cursor_color & 0xF, v.text.cursor_color >> 4));
        const unsigned ch = v.text.cursor_char;
        u8("CURSOR CHAR", ch, ch >= 32 && ch < 127 ? fmt("'%c'", (char) ch) : "");
        u8("BLINK TIME", v.text.cursor_time, "frames");
        u16("CURSOR OFS", v.text.cursor_addr, "tilemap");
        text("GRID", fmt("%ux%u", v.text.columns, v.text.lines));
        chips("FLAGS", { { "SCRX", v.text.auto_scroll_x }, { "SCRY", v.text.auto_scroll_y },
                         { "WRAP", v.text.wait_on_wrap }, { "SCROLLED", v.text.scroll_y_occurred } })
            .tip = "Auto scroll X/Y, wait on wrap, Y scroll occurred";
        chips("CURSOR", { { "SHOWN", v.text.cursor_shown }, { "PENDING", v.text.wait_next_char } });

        section("PALETTE");
        swatches("COLORS 0-15", v.palette, FZ_HW_PALETTE_SHOWN);
        u8("WR LATCH", (unsigned) v.palette_latch);

        section("SPRITES");
        u8("ON SCREEN", (unsigned) v.sprites_on_screen, "of 128");
        u8("WR LATCH", (unsigned) v.sprites_latch);

        section("DMA CONTROLLER");
        phys("DESCRIPTOR", v.dma.desc_addr);
        text("STATE", name_of(s_dma_states, 6, v.dma.state));
        text("CLOCK", fmt("rd %u / wr %u cycles", v.dma.rd_cycles, v.dma.wr_cycles));
        phys("READ PTR", v.dma.rd_addr);
        phys("WRITE PTR", v.dma.wr_addr);
        u16("REMAINING", v.dma.remaining, "bytes");
        u8("FLAGS", v.dma.flags);
        chips("BUS", { { "WIRED", v.dma.bus_connected } });

        section("RASTER PROCESSOR");
        u8("PC", v.rpu.pc);
        text("STATE", name_of(s_rpu_states, 3, v.rpu.state));
        row("INSTRUCTION", hexn(v.rpu.instruction, 8), "");
        u16("HMASK", v.rpu.hmask);
        u16("VMASK", v.rpu.vmask);
        u8("UPLOAD ADDR", v.rpu.upload_addr);
        chips("IRQ", { { "ENABLE", v.rpu.int_enabled }, { "RAISED", v.rpu.int_raw } });

        section("TIMER");
        chips("CONTROL", { { "EN", v.timer.enabled }, { "RELOAD", v.timer.auto_reload },
                           { "DEC", v.timer.decrement }, { "IRQ", v.timer.int_enabled } });
        u16("DIVIDER", v.timer.divider);
        u16("RELOAD", v.timer.reload);
        u16("COUNTER", v.timer.counter);
        chips("STATUS", { { "PENDING", v.timer.int_pending } });

        section("CRC32");
        row("SUM", hexn(v.crc32, 8), "");

        section("SPI / TF CARD");
        u8("CLK DIV", v.spi.clk_div);
        u8("RAM LEN", v.spi.ram_len);
        u8("OPERAND", v.spi.operand);
        chips("STATE", { { "BUSY", v.spi.busy }, { "TF CS", v.spi.tf_cs } });
        text("TRANSFER", fmt("%u / %u", v.spi.transfer_index, v.spi.transfer_len));
        text("RAM IDX", fmt("rd %u  wr %u", v.spi.ram_rd_idx, v.spi.ram_wr_idx));
        text("TF STATE", name_of(s_tf_states, 7, v.spi.tf_state));
        u8("TF COMMAND", v.spi.tf_command, fmt("CMD%u", v.spi.tf_command & 0x3F));
        row("TF ARGUMENT", hexn(v.spi.tf_argument, 8), dec(v.spi.tf_argument));
        text("TF CARD", v.spi.tf_size ? fmt("%llu KB", (unsigned long long) (v.spi.tf_size / 1024)) : "none");
    }
};

static Panel* make_hw_video(int x, int y, int w, int h)
{
    return new VideoRegistersView(x, y, w, h);
}

static ToolViewRegistrar s_reg(TOOL_HARDWARE, 10, "Video", make_hw_video);

#endif

} // namespace zui
