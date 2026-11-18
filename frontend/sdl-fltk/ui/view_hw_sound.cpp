/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Hardware window: ZVB sound controller (4 voices + sample table). */

#include "hw_view.h"

namespace zui {

#if CONFIG_ENABLE_DEBUGGER

namespace {

const char* const s_waves[] = { "SQUARE", "TRIANGLE", "SAWTOOTH", "NOISE" };
const char* const s_voice_sections[] = { "VOICE 0", "VOICE 1", "VOICE 2", "VOICE 3" };

std::vector<HwRegisterView::Chip> voice_bits(uint8_t reg)
{
    return { { "V0", (reg & 1) != 0 }, { "V1", (reg & 2) != 0 }, { "V2", (reg & 4) != 0 },
             { "V3", (reg & 8) != 0 }, { "SMP", (reg & 0x80) != 0 } };
}

} // namespace

class SoundRegistersView : public HwRegisterView {
public:
    SoundRegistersView(int X, int Y, int W, int H) : HwRegisterView(X, Y, W, H, "SOUND") {}

protected:
    void build() override
    {
        fz_hw_sound_t s;
        if (!fz_hw_sound(&s)) {
            section("NO MACHINE");
            return;
        }

        section("MASTER");
        u8("SELECT", s.enabled_reg).tip = "Voices receiving register writes";
        chips("SELECTED", voice_bits(s.enabled_reg));
        u8("LEFT", s.left_reg);
        chips("ROUTED L", voice_bits(s.left_reg));
        u8("RIGHT", s.right_reg);
        chips("ROUTED R", voice_bits(s.right_reg));
        u8("HOLD", s.hold_reg);
        chips("HELD", voice_bits(s.hold_reg));
        const unsigned mv = s.master_volume;
        u8("VOLUME", mv, fmt("L %u%%  R %u%%", ((mv & 3) + 1) * 25, (((mv >> 2) & 3) + 1) * 25));
        chips("MUTE", { { "LEFT", (mv & 0x40) != 0 }, { "RIGHT", (mv & 0x80) != 0 } });
        text("OUTPUT", fmt("L %d  R %d", s.last_left, s.last_right));

        for (int i = 0; i < FZ_HW_VOICES; i++) {
            const auto& v = s.voice[i];
            section(s_voice_sections[i]);
            u16("FREQUENCY", v.freq_active, v.wave == 3 ? std::string("noise") : fmt("%.1f Hz", v.freq_hz));
            Item& reg = u16("FREQ REG", v.freq_reg, v.pending_reload ? "pending" : "");
            reg.tip = "Written value, applied at the next waveform restart";
            Item& wave = u8("WAVEFORM", v.wave, name_of(s_waves, 4, v.wave));
            wave.glyph = v.wave;
            wave.duty = 8 - (v.duty & 7);
            u8("DUTY", v.duty, v.wave == 0 ? fmt("high %u/8", 8u - (v.duty & 7)) : "n/a");
            u8("VOLUME", v.volume, (v.volume & 0x80) ? std::string("MUTED") : fmt("%u%%", ((v.volume & 3) + 1) * 25));
            chips("STATE", { { "L", v.left }, { "R", v.right }, { "HOLD", v.hold }, { "SEL", v.selected } });
            Item& out = u16("LEVEL", v.output);
            out.level = v.output / 65535.0f;
        }

        section("SAMPLE TABLE");
        u8("CONFIG", s.sample.config,
           s.sample.is_u8 ? "8-bit unsigned" : s.sample.is_signed ? "16-bit signed" : "16-bit unsigned");
        u8("DIVIDER", s.sample.divider, fmt("%.0f Hz", s.sample.rate_hz));
        u16("FIFO", (unsigned) s.sample.fifo_count, "of 256");
        text("HEAD / TAIL", fmt("%d / %d", s.sample.fifo_head, s.sample.fifo_tail));
        chips("FLAGS", { { "EMPTY", s.sample.fifo_empty }, { "LOOP", s.sample.loop },
                         { "IRQ EN", s.sample.irq_enable }, { "IRQ", s.sample.int_pending } });
        chips("STATE", { { "L", s.sample.routed_left }, { "R", s.sample.routed_right },
                         { "HOLD", s.sample.hold }, { "SEL", s.sample.selected } });
        Item& out = u16("LEVEL", s.sample.output);
        out.level = s.sample.output / 65535.0f;
    }
};

static Panel* make_hw_sound(int x, int y, int w, int h)
{
    return new SoundRegistersView(x, y, w, h);
}

static ToolViewRegistrar s_reg(TOOL_HARDWARE, 20, "Sound", make_hw_sound);

#endif

} // namespace zui
