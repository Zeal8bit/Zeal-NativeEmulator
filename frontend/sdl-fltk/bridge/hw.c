/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Machine-side implementation of bridge/hw.h: read-only register snapshots. */

#include <string.h>
#include <time.h>
#include "frontend/sdl-fltk/frontend.h"
#include "frontend/sdl-fltk/bridge/hw.h"

#define PIN(port, n)    ((((port).state) >> (n)) & 1)

static unsigned bcd(uint8_t v)
{
    return (v >> 4) * 10 + (v & 0xF);
}

/* ------------------------------------------------------------------ */
/*  Video                                                              */
/* ------------------------------------------------------------------ */

bool fz_hw_video(fz_hw_video_t* out)
{
    memset(out, 0, sizeof(*out));
    if (fltk_machine == NULL) return false;
    const zvb_t* zvb = &fltk_machine->zvb;

    out->rev = ZVB_EMULATED_REV;
    out->minor = ZVB_EMULATED_MINOR;
    out->major = ZVB_EMULATED_MAJOR;
    out->mode = (uint8_t) zvb->mode;
    out->status = zvb->status.raw;
    out->vid_ena = zvb->status.vid_ena;
    out->h_blank = zvb->status.h_blank;
    out->v_blank = zvb->status.v_blank;
    out->h_int_ena = zvb->status.h_int_ena;
    out->v_int_ena = zvb->status.v_int_ena;
    out->screen_enabled = zvb->screen_enabled;
    out->blank_latches = zvb->blank_latches;
    out->ext_irq = (uint8_t) ((zvb_sound_interrupt(&zvb->sound) ? 1 : 0) |
                              (zvb_rpu_interrupt(&zvb->rpu) ? 2 : 0) |
                              (zvb_timer_interrupt(&zvb->peri_timer) ? 4 : 0));
    out->gp_irq = out->ext_irq || (zvb->status.h_int_ena && (zvb->blank_latches & 1));
    out->io_bank = zvb->io_bank;
    memcpy(out->scratch, zvb->scratch, sizeof(out->scratch));
    out->scanline = zvb->current_scanline;
    out->hpos = zvb->current_hpos;
    out->raster_state = zvb->state;
#if ZVB_SCANLINE_RENDERING
    out->scanline_rendering = zvb->scanline_rendering;
#endif

    out->l0_scroll_x = (uint16_t) zvb->ctrl.l0_scroll_x;
    out->l0_scroll_y = (uint16_t) zvb->ctrl.l0_scroll_y;
    out->l1_scroll_x = (uint16_t) zvb->ctrl.l1_scroll_x;
    out->l1_scroll_y = (uint16_t) zvb->ctrl.l1_scroll_y;

    const zvb_text_t* t = &zvb->text;
    out->text.cursor_x = t->cursor_pos.x;
    out->text.cursor_y = t->cursor_pos.y;
    out->text.save_x = t->cursor_save.x;
    out->text.save_y = t->cursor_save.y;
    out->text.scroll_x = t->scroll.x;
    out->text.scroll_y = t->scroll.y;
    out->text.color = t->color;
    out->text.cursor_color = t->cursor_color;
    out->text.cursor_char = t->cursor_char;
    out->text.cursor_time = t->cursor_time;
    out->text.auto_scroll_x = t->flags.auto_scroll_x;
    out->text.auto_scroll_y = t->flags.auto_scroll_y;
    out->text.wait_on_wrap = t->flags.wait_on_wrap;
    out->text.scroll_y_occurred = t->flags.scroll_y_occurred;
    out->text.wait_next_char = t->wait_for_next_char;
    out->text.cursor_shown = t->cursor_shown;
    out->text.columns = t->visible_columns;
    out->text.lines = t->visible_lines;
    out->text.cursor_addr = (uint16_t) zvb_text_cursor_address(t);

    for (int i = 0; i < FZ_HW_PALETTE_SHOWN; i++) {
        out->palette[i] = (uint16_t) (zvb->palette.raw_palette[i * 2] | (zvb->palette.raw_palette[i * 2 + 1] << 8));
    }
    out->palette_latch = zvb->palette.wr_latch;
    out->sprites_latch = zvb->sprites.wr_latch;
    const bool small = zvb->mode == MODE_GFX_320_8BIT || zvb->mode == MODE_GFX_320_4BIT;
    const int sw = small ? 320 : 640, sh = small ? 240 : 480;
    for (int i = 0; i < ZVB_SPRITES_COUNT; i++) {
        const zvb_sprite_t* s = &zvb->sprites.data[i];
        const int h = s->extra_flags.bitmap.height_32 ? 32 : 16;
        const int x = (int) s->x - 16, y = (int) s->y - 16;
        if (x > -16 && x < sw && y > -h && y < sh) out->sprites_on_screen++;
    }

    const zvb_dma_t* dma = &zvb->dma;
    out->dma.desc_addr = dma->desc_addr & 0xFFFFFF;
    out->dma.rd_cycles = dma->clk.rd_cycle;
    out->dma.wr_cycles = dma->clk.wr_cycle;
    out->dma.state = (int) dma->state;
    out->dma.rd_addr = dma->rd_addr;
    out->dma.wr_addr = dma->wr_addr;
    out->dma.remaining = dma->remaining;
    out->dma.flags = dma->flags;
    out->dma.bus_connected = dma->bus_connected;

    const zvb_rpu_t* rpu = &zvb->rpu;
    out->rpu.pc = rpu->pc;
    out->rpu.state = rpu->state;
    out->rpu.upload_addr = rpu->upload_addr;
    out->rpu.int_enabled = rpu->int_enabled;
    out->rpu.int_raw = rpu->int_raw;
    out->rpu.hmask = rpu->hmask;
    out->rpu.vmask = rpu->vmask;
    out->rpu.instruction = rpu->instruction;

    const zvb_timer_t* tm = &zvb->peri_timer;
    out->timer.enabled = tm->enabled;
    out->timer.auto_reload = tm->auto_reload;
    out->timer.decrement = tm->decrement;
    out->timer.int_enabled = tm->int_enabled;
    out->timer.int_pending = tm->int_pending;
    out->timer.divider = tm->divider;
    out->timer.reload = tm->reload;
    out->timer.counter = tm->counter;

    out->crc32 = zvb->peri_crc32.sum;

    const zvb_spi_t* spi = &zvb->spi;
    out->spi.clk_div = spi->clk_div;
    out->spi.ram_len = spi->ram_len;
    out->spi.operand = spi->operand;
    out->spi.busy = spi->busy;
    out->spi.tf_cs = spi->tf_cs != 0;
    out->spi.transfer_index = spi->transfer_index;
    out->spi.transfer_len = spi->transfer_len;
    out->spi.ram_rd_idx = spi->ram_rd.idx;
    out->spi.ram_wr_idx = spi->ram_wr.idx;
    out->spi.tf_state = (int) spi->tf.state;
    out->spi.tf_command = (uint8_t) spi->tf.debug_command;
    out->spi.tf_argument = spi->tf.debug_parameter;
    out->spi.tf_size = spi->tf.backend.size;
    return true;
}

/* ------------------------------------------------------------------ */
/*  Sound                                                              */
/* ------------------------------------------------------------------ */

bool fz_hw_sound(fz_hw_sound_t* out)
{
    memset(out, 0, sizeof(*out));
    if (fltk_machine == NULL) return false;
    const zvb_sound_t* s = &fltk_machine->zvb.sound;

    for (int i = 0; i < FZ_HW_VOICES; i++) {
        const zvb_voice_t* v = &s->voices[i];
        out->voice[i].freq_reg = (uint16_t) ((v->freq_high << 8) | v->freq_low);
        out->voice[i].freq_active = v->frequency;
        /* The phase accumulator overflows at 2^16, advanced once per sample */
        out->voice[i].freq_hz = (double) v->frequency * SAMPLE_RATE / 65536.0;
        out->voice[i].wave = v->wave;
        out->voice[i].duty = v->duty;
        out->voice[i].volume = v->voice_volume;
        out->voice[i].hold = v->hold;
        out->voice[i].pending_reload = v->need_reload;
        out->voice[i].selected = (s->enabled_voices >> i) & 1;
        out->voice[i].left = (s->left_voices >> i) & 1;
        out->voice[i].right = (s->right_voices >> i) & 1;
        out->voice[i].output = (uint16_t) atomic_load_explicit(&((zvb_voice_t*) v)->output, memory_order_relaxed);
    }
    out->enabled_reg = (uint8_t) s->enabled_voices;
    out->left_reg = (uint8_t) s->left_voices;
    out->right_reg = (uint8_t) s->right_voices;
    out->hold_reg = (uint8_t) s->hold_voices;
    out->master_volume = (uint8_t) s->master_volume;

    const zvb_sample_table_t* t = &s->sample_table;
    out->sample.selected = (s->enabled_voices & 0x80) != 0;
    out->sample.hold = t->hold;
    out->sample.routed_left = (s->left_voices & 0x80) != 0;
    out->sample.routed_right = (s->right_voices & 0x80) != 0;
    out->sample.divider = (uint8_t) t->divider;
    out->sample.config = (uint8_t) t->config;
    out->sample.is_u8 = t->is_u8;
    out->sample.is_signed = t->is_signed;
    out->sample.loop = (t->config & 2) != 0;
    out->sample.irq_enable = (t->config & 8) != 0;
    out->sample.int_pending = t->int_pending;
    out->sample.fifo_empty = t->fifo_empty;
    out->sample.fifo_head = t->fifo_head;
    out->sample.fifo_tail = t->fifo_tail;
    int count = (t->fifo_head - t->fifo_tail + SAMPLE_FIFO_SIZE) % SAMPLE_FIFO_SIZE;
    if (count == 0 && !t->fifo_empty) count = SAMPLE_FIFO_SIZE;
    out->sample.fifo_count = count;
    out->sample.rate_hz = (double) SAMPLE_RATE / (t->divider + 1);
    out->sample.output = (uint16_t) atomic_load_explicit(&((zvb_sample_table_t*) t)->output, memory_order_relaxed);

    out->last_left = s->output_last.left;
    out->last_right = s->output_last.right;
    out->sample_rate = SAMPLE_RATE;
    return true;
}

/* ------------------------------------------------------------------ */
/*  I/O                                                                */
/* ------------------------------------------------------------------ */

bool fz_hw_io(fz_hw_io_t* out)
{
    memset(out, 0, sizeof(*out));
    if (fltk_machine == NULL) return false;
    const zeal_t* m = fltk_machine;

    const port_t* ports[2] = { &m->pio.port_a, &m->pio.port_b };
    for (int i = 0; i < 2; i++) {
        out->pio[i].mode = ports[i]->mode;
        out->pio[i].state = ports[i]->state;
        out->pio[i].dir = ports[i]->dir;
        out->pio[i].int_vector = ports[i]->int_vector;
        out->pio[i].int_mask = ports[i]->int_mask;
        out->pio[i].int_enable = ports[i]->int_enable;
        out->pio[i].and_op = ports[i]->and_op;
        out->pio[i].active_high = ports[i]->active_high;
    }
    const port_t b = m->pio.port_b;

    out->uart.tx_pos = m->uart.tx_pos;
    for (int i = 1; i < m->uart.tx_pos && i < 9; i++) {
        out->uart.tx_bits |= (uint8_t) ((m->uart.tx_fifo[i] & 1) << (i - 1));
    }
    out->uart.tx_pin = PIN(b, IO_UART_TX_PIN);
    out->uart.rx_pin = PIN(b, IO_UART_RX_PIN);

    out->i2c.state = (int) m->i2c_bus.st;
    out->i2c.dev_addr = m->i2c_bus.dev_addr;
    out->i2c.cur_byte = m->i2c_bus.cur_byte;
    out->i2c.cur_bit = m->i2c_bus.cur_bit;
    out->i2c.has_reply = m->i2c_bus.has_reply;
    out->i2c.scl = PIN(b, IO_I2C_SCL_OUT_PIN);
    out->i2c.sda_out = PIN(b, IO_I2C_SDA_OUT_PIN);
    out->i2c.sda_in = PIN(b, IO_I2C_SDA_IN_PIN);

    out->rtc.reg = m->rtc.reg;
    out->rtc.count = m->rtc.count;
    memcpy(out->rtc.ram, m->rtc.ram, sizeof(out->rtc.ram));
    out->rtc.offset_s = m->rtc.time_diff;
    out->rtc.halted = (m->rtc.ram[0] & 0x80) != 0;
    /* Same computation as the RTC backend (frontend/common/rtc.c) */
    const time_t now = time(NULL) + (time_t) m->rtc.time_diff;
    const struct tm* tm = localtime(&now);
    if (tm) {
        out->rtc.year = tm->tm_year + 1900;
        out->rtc.month = tm->tm_mon + 1;
        out->rtc.day = tm->tm_mday;
        out->rtc.wday = tm->tm_wday;
        out->rtc.hour = tm->tm_hour;
        out->rtc.minute = tm->tm_min;
        out->rtc.second = tm->tm_sec;
    } else {
        out->rtc.year = 2000 + (int) bcd(m->rtc.ram[6]);
        out->rtc.month = (int) bcd(m->rtc.ram[5] & 0x1F);
        out->rtc.day = (int) bcd(m->rtc.ram[4] & 0x3F);
        out->rtc.wday = (int) bcd(m->rtc.ram[3] & 7) - 1;
        out->rtc.hour = (int) bcd(m->rtc.ram[2] & 0x3F);
        out->rtc.minute = (int) bcd(m->rtc.ram[1] & 0x7F);
        out->rtc.second = (int) bcd(m->rtc.ram[0] & 0x7F);
    }

    out->eeprom.address = m->eeprom.address;
    out->eeprom.count = m->eeprom.count;
    out->eeprom.writing = m->eeprom.writing;
    out->eeprom.sector_written = m->eeprom.sector_written;

    out->keyboard.state = (int) m->keyboard.state;
    out->keyboard.shift_register = m->keyboard.shift_register;
    out->keyboard.pin = m->keyboard.pin_state != 0;
    out->keyboard.fifo_count = (int) fifo_size((fifo_t*) &m->keyboard.queue);

    const compactflash_t* cf = &m->compactflash;
    out->cf.present = m->cpu.mmu.io_mapping[0x70].dev == &cf->parent;
    out->cf.failed = cf->failed;
    out->cf.lba_mode = cf->lba_mode;
    out->cf.master = cf->master;
    out->cf.state = (int) cf->state;
    out->cf.status = cf->status;
    out->cf.error = cf->error;
    out->cf.feature = cf->feature;
    out->cf.sec_cnt = cf->sec_cnt;
    out->cf.sec_cur = cf->sec_cur;
    out->cf.buffer_idx = cf->sector_buffer_idx;
    out->cf.lba = (uint32_t) cf->lba_0 | ((uint32_t) cf->lba_8 << 8) | ((uint32_t) cf->lba_16 << 16) |
                  ((uint32_t) (cf->lba_24 & 0x0F) << 24);
    out->cf.total_sectors = cf->total_sectors;

    memcpy(out->hostfs.regs, m->hostfs.registers, sizeof(out->hostfs.regs));
    out->hostfs.bound = m->hostfs.dispatch != NULL;

    for (int i = 0; i < MMU_PAGES_COUNT && i < 4; i++) {
        out->mmu.pages[i] = m->cpu.mmu.pages[i];
        const device_t* dev = m->cpu.mmu.vpages[i].dev;
        out->mmu.device[i] = dev ? dev->name : NULL;
    }
    return true;
}

int32_t fz_hw_phys_to_virt(uint32_t phys)
{
    if (fltk_machine == NULL) return -1;
    const mmu_t* mmu = &fltk_machine->cpu.mmu;
    for (int i = 0; i < MMU_PAGES_COUNT; i++) {
        if (mmu->pages[i] == (phys >> 14)) return (int32_t) ((i << 14) | (phys & 0x3FFF));
    }
    return -1;
}
