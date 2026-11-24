/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware/video_registers_view.h"
#include "common/machine.h"
#include "frontend/sdl-fltk/frontend.h"
#include "hardware/hw_register_view.h"
#include "tools/tool_registry.h"
#include "widgets/panel.h"

namespace zui::hardware {


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

static widgets::Panel* make_hw_video(int x, int y, int w, int h)
{
    return new VideoRegistersView(x, y, w, h);
}

static tools::ToolViewRegistrar s_reg(tools::TOOL_HARDWARE, 10, "Video", make_hw_video);

VideoRegistersView::VideoRegistersView(int X, int Y, int W, int H)
    : HwRegisterView(X, Y, W, H, "VIDEO BOARD")
{}

void VideoRegistersView::build()
{
    if (fltk_machine == nullptr) {
        section("NO MACHINE");
        return;
    }
    const zvb_t& v = fltk_machine->zvb;
    const ModeInfo* mode = (int) v.mode < 8 ? &s_modes[(int) v.mode] : nullptr;

    section("VIDEO MODE");
    text("VERSION", fmt("v%u.%u.%u", ZVB_EMULATED_MAJOR, ZVB_EMULATED_MINOR, ZVB_EMULATED_REV));
    u8("MODE", (unsigned) v.mode, mode ? mode->name : "?");
    text("RESOLUTION", mode ? mode->resolution : "?");
    text("COLORS", mode ? mode->colors : "?");
    u8("STATUS", v.status.raw);
    chips("CONTROL", { { "VID", v.status.vid_ena != 0 }, { "HINT", v.status.h_int_ena != 0 },
                       { "VINT", v.status.v_int_ena != 0 }, { "OUT", v.screen_enabled } })
        .tip = "Video enable, h/v-blank interrupt enables, output latched at vblank";
    u8("IO BANK", v.io_bank, name_of(s_banks, 7, v.io_bank));
    text("SCRATCH", h8(v.scratch[0]) + " " + h8(v.scratch[1]) + " " + h8(v.scratch[2]) + " " + h8(v.scratch[3]));

    const uint8_t ext_irq = (uint8_t) ((zvb_sound_interrupt(&v.sound) ? 1 : 0) |
                                       (zvb_rpu_interrupt(&v.rpu) ? 2 : 0) |
                                       (zvb_timer_interrupt(&v.peri_timer) ? 4 : 0));
    const bool gp_irq = ext_irq != 0 || (v.status.h_int_ena && (v.blank_latches & 1) != 0);

    section("RASTER");
    u16("SCANLINE", (unsigned) v.current_scanline);
    u16("HPOS", (unsigned) v.current_hpos);
    text("STATE", v.state ? "HBLANK" : "RENDERING");
    chips("BLANK", { { "HBL", v.status.h_blank != 0 }, { "VBL", v.status.v_blank != 0 } });
    chips("LATCHED", { { "HBL", (v.blank_latches & 1) != 0 }, { "VBL", (v.blank_latches & 2) != 0 } });
    chips("IRQ", { { "GP", gp_irq }, { "SND", (ext_irq & 1) != 0 }, { "RPU", (ext_irq & 2) != 0 },
                   { "TMR", (ext_irq & 4) != 0 } });
    chips("RENDERER", { { "SCANLINE", v.scanline_rendering } });

    section("LAYERS");
    u16("L0 SCROLL X", v.ctrl.l0_scroll_x);
    u16("L0 SCROLL Y", v.ctrl.l0_scroll_y);
    u16("L1 SCROLL X", v.ctrl.l1_scroll_x);
    u16("L1 SCROLL Y", v.ctrl.l1_scroll_y);

    const zvb_text_t& t = v.text;
    section("TEXT CONTROLLER");
    u8("CURSOR X", t.cursor_pos.x);
    u8("CURSOR Y", t.cursor_pos.y);
    text("SAVED", fmt("%u, %u", t.cursor_save.x, t.cursor_save.y));
    u8("SCROLL X", t.scroll.x);
    u8("SCROLL Y", t.scroll.y);
    u8("COLOR", t.color, fmt("fg %u bg %u", t.color & 0xF, t.color >> 4));
    u8("CURSOR COLOR", t.cursor_color, fmt("fg %u bg %u", t.cursor_color & 0xF, t.cursor_color >> 4));
    const unsigned ch = t.cursor_char;
    u8("CURSOR CHAR", ch, ch >= 32 && ch < 127 ? fmt("'%c'", (char) ch) : "");
    u8("BLINK TIME", t.cursor_time, "frames");
    u16("CURSOR OFS", zvb_text_cursor_address(&t), "tilemap");
    text("GRID", fmt("%ux%u", t.visible_columns, t.visible_lines));
    chips("FLAGS", { { "SCRX", t.flags.auto_scroll_x != 0 }, { "SCRY", t.flags.auto_scroll_y != 0 },
                     { "WRAP", t.flags.wait_on_wrap != 0 }, { "SCROLLED", t.flags.scroll_y_occurred != 0 } })
        .tip = "Auto scroll X/Y, wait on wrap, Y scroll occurred";
    chips("CURSOR", { { "SHOWN", t.cursor_shown }, { "PENDING", t.wait_for_next_char } });

    uint16_t palette[16];
    for (int i = 0; i < 16; i++) {
        palette[i] = (uint16_t) (v.palette.raw_palette[i * 2] | (v.palette.raw_palette[i * 2 + 1] << 8));
    }
    section("PALETTE");
    swatches("COLORS 0-15", palette, 16);
    u8("WR LATCH", (unsigned) v.palette.wr_latch);

    const bool small = v.mode == MODE_GFX_320_8BIT || v.mode == MODE_GFX_320_4BIT;
    const int sw = small ? 320 : 640, sh = small ? 240 : 480;
    int sprites_on_screen = 0;
    for (int i = 0; i < ZVB_SPRITES_COUNT; i++) {
        const zvb_sprite_t* sp = &v.sprites.data[i];
        const int h = sp->extra_flags.bitmap.height_32 ? 32 : 16;
        const int x = (int) sp->x - 16, y = (int) sp->y - 16;
        if (x > -16 && x < sw && y > -h && y < sh) {
            sprites_on_screen++;
        }
    }
    section("SPRITES");
    u8("ON SCREEN", (unsigned) sprites_on_screen, "of 128");
    u8("WR LATCH", (unsigned) v.sprites.wr_latch);

    const zvb_dma_t& dma = v.dma;
    section("DMA CONTROLLER");
    phys("DESCRIPTOR", dma.desc_addr & 0xFFFFFF);
    text("STATE", name_of(s_dma_states, 6, (int) dma.state));
    text("CLOCK", fmt("rd %u / wr %u cycles", (unsigned) dma.clk.rd_cycle, (unsigned) dma.clk.wr_cycle));
    phys("READ PTR", dma.rd_addr);
    phys("WRITE PTR", dma.wr_addr);
    u16("REMAINING", dma.remaining, "bytes");
    u8("FLAGS", dma.flags);
    chips("BUS", { { "WIRED", dma.bus_connected } });

    const zvb_rpu_t& rpu = v.rpu;
    section("RASTER PROCESSOR");
    u8("PC", rpu.pc);
    text("STATE", name_of(s_rpu_states, 3, rpu.state));
    row("INSTRUCTION", hexn(rpu.instruction, 8), "");
    u16("HMASK", rpu.hmask);
    u16("VMASK", rpu.vmask);
    u8("UPLOAD ADDR", rpu.upload_addr);
    chips("IRQ", { { "ENABLE", rpu.int_enabled }, { "RAISED", rpu.int_raw } });

    const zvb_timer_t& tm = v.peri_timer;
    section("TIMER");
    chips("CONTROL", { { "EN", tm.enabled }, { "RELOAD", tm.auto_reload },
                       { "DEC", tm.decrement }, { "IRQ", tm.int_enabled } });
    u16("DIVIDER", tm.divider);
    u16("RELOAD", tm.reload);
    u16("COUNTER", tm.counter);
    chips("STATUS", { { "PENDING", tm.int_pending } });

    section("CRC32");
    row("SUM", hexn(v.peri_crc32.sum, 8), "");

    const zvb_spi_t& spi = v.spi;
    section("SPI / TF CARD");
    u8("CLK DIV", spi.clk_div);
    u8("RAM LEN", spi.ram_len);
    u8("OPERAND", spi.operand);
    chips("STATE", { { "BUSY", spi.busy }, { "TF CS", spi.tf_cs != 0 } });
    text("TRANSFER", fmt("%u / %u", (unsigned) spi.transfer_index, (unsigned) spi.transfer_len));
    text("RAM IDX", fmt("rd %u  wr %u", (unsigned) spi.ram_rd.idx, (unsigned) spi.ram_wr.idx));
    text("TF STATE", name_of(s_tf_states, 7, (int) spi.tf.state));
    u8("TF COMMAND", (unsigned) spi.tf.debug_command, fmt("CMD%u", (unsigned) (spi.tf.debug_command & 0x3F)));
    row("TF ARGUMENT", hexn(spi.tf.debug_parameter, 8), dec(spi.tf.debug_parameter));
    text("TF CARD", spi.tf.backend.size ? fmt("%llu KB", (unsigned long long) (spi.tf.backend.size / 1024)) : "none");
}

} // namespace zui::hardware
