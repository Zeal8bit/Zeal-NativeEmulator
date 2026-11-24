/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware/sound_registers_view.h"
#include <cstdint>
#include <string>
#include <vector>
#include "common/machine.h"
#include "frontend/sdl-fltk/frontend.h"
#include "hardware/hw_register_view.h"
#include "tools/tool_registry.h"
#include "widgets/panel.h"

namespace zui::hardware {


const char* const s_waves[] = { "SQUARE", "TRIANGLE", "SAWTOOTH", "NOISE" };
const char* const s_voice_sections[] = { "VOICE 0", "VOICE 1", "VOICE 2", "VOICE 3" };

static std::vector<HwRegisterView::Chip> voice_bits(uint8_t reg)
{
    return { { "V0", (reg & 1) != 0 }, { "V1", (reg & 2) != 0 }, { "V2", (reg & 4) != 0 },
             { "V3", (reg & 8) != 0 }, { "SMP", (reg & 0x80) != 0 } };
}

static widgets::Panel* make_hw_sound(int x, int y, int w, int h)
{
    return new SoundRegistersView(x, y, w, h);
}

static tools::ToolViewRegistrar s_reg(tools::TOOL_HARDWARE, 20, "Sound", make_hw_sound);

SoundRegistersView::SoundRegistersView(int X, int Y, int W, int H)
    : HwRegisterView(X, Y, W, H, "SOUND")
{}

void SoundRegistersView::build()
{
    if (fltk_machine == nullptr) {
        section("NO MACHINE");
        return;
    }
    const zvb_sound_t& s = fltk_machine->zvb.sound;

    section("MASTER");
    u8("SELECT", s.enabled_voices).tip = "Voices receiving register writes";
    chips("SELECTED", voice_bits((uint8_t) s.enabled_voices));
    u8("LEFT", s.left_voices);
    chips("ROUTED L", voice_bits((uint8_t) s.left_voices));
    u8("RIGHT", s.right_voices);
    chips("ROUTED R", voice_bits((uint8_t) s.right_voices));
    u8("HOLD", s.hold_voices);
    chips("HELD", voice_bits((uint8_t) s.hold_voices));
    const unsigned mv = s.master_volume;
    u8("VOLUME", mv, fmt("L %u%%  R %u%%", ((mv & 3) + 1) * 25, (((mv >> 2) & 3) + 1) * 25));
    chips("MUTE", { { "LEFT", (mv & 0x40) != 0 }, { "RIGHT", (mv & 0x80) != 0 } });
    text("OUTPUT", fmt("L %d  R %d", s.output_last.left, s.output_last.right));

    for (int i = 0; i < VOICE_COUNT; i++) {
        const zvb_voice_t& v = s.voices[i];
        const double freq_hz = (double) v.frequency * SAMPLE_RATE / 65536.0;
        const uint16_t freq_reg = (uint16_t) ((v.freq_high << 8) | v.freq_low);
        section(s_voice_sections[i]);
        u16("FREQUENCY", v.frequency, v.wave == 3 ? std::string("noise") : fmt("%.1f Hz", freq_hz));
        Item& reg = u16("FREQ REG", freq_reg, v.need_reload ? "pending" : "");
        reg.tip = "Written value, applied at the next waveform restart";
        Item& wave = u8("WAVEFORM", v.wave, name_of(s_waves, 4, v.wave));
        wave.glyph = v.wave;
        wave.duty = 8 - (v.duty & 7);
        u8("DUTY", v.duty, v.wave == 0 ? fmt("high %u/8", 8u - (v.duty & 7)) : "n/a");
        u8("VOLUME", v.voice_volume,
           (v.voice_volume & 0x80) ? std::string("MUTED") : fmt("%u%%", ((v.voice_volume & 3) + 1) * 25));
        chips("STATE", { { "L", ((s.left_voices >> i) & 1) != 0 }, { "R", ((s.right_voices >> i) & 1) != 0 },
                         { "HOLD", v.hold }, { "SEL", ((s.enabled_voices >> i) & 1) != 0 } });
        const uint16_t level = (uint16_t) __atomic_load_n((int*) (void*) &v.output, __ATOMIC_RELAXED);
        Item& out = u16("LEVEL", level);
        out.level = level / 65535.0f;
    }

    section("SAMPLE TABLE");
    const zvb_sample_table_t& t = s.sample_table;
    u8("CONFIG", t.config,
       t.is_u8 ? "8-bit unsigned" : t.is_signed ? "16-bit signed" : "16-bit unsigned");
    u8("DIVIDER", t.divider, fmt("%.0f Hz", (double) SAMPLE_RATE / (t.divider + 1)));
    int count = (t.fifo_head - t.fifo_tail + SAMPLE_FIFO_SIZE) % SAMPLE_FIFO_SIZE;
    if (count == 0 && !t.fifo_empty) {
        count = SAMPLE_FIFO_SIZE;
    }
    u16("FIFO", (unsigned) count, "of 256");
    text("HEAD / TAIL", fmt("%d / %d", t.fifo_head, t.fifo_tail));
    chips("FLAGS", { { "EMPTY", t.fifo_empty }, { "LOOP", (t.config & 2) != 0 },
                     { "IRQ EN", (t.config & 8) != 0 }, { "IRQ", t.int_pending } });
    chips("STATE", { { "L", (s.left_voices & 0x80) != 0 }, { "R", (s.right_voices & 0x80) != 0 },
                     { "HOLD", t.hold }, { "SEL", (s.enabled_voices & 0x80) != 0 } });
    const uint16_t sample_level = (uint16_t) __atomic_load_n(&t.output, __ATOMIC_RELAXED);
    Item& out = u16("LEVEL", sample_level);
    out.level = sample_level / 65535.0f;
}

} // namespace zui::hardware
