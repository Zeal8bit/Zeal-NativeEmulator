/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Read-only snapshots of the hardware registers for the Hardware tool
 * window (video board, sound, I/O devices). Plain C data so that the C++ UI
 * never includes hw/zeal.h. Filling a snapshot never changes machine state. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Zeal Video Board                                                   */
/* ------------------------------------------------------------------ */

#define FZ_HW_PALETTE_SHOWN 16

typedef struct {
    uint8_t  rev, minor, major;
    uint8_t  mode;             /* zvb_video_mode_t */
    uint8_t  status;           /* raw status register */
    bool     vid_ena, h_blank, v_blank, h_int_ena, v_int_ena;
    bool     screen_enabled;   /* VGA enable latched during vertical blank */
    uint8_t  blank_latches;    /* bit 0 hblank, bit 1 vblank */
    bool     gp_irq;           /* general purpose interrupt line asserted */
    uint8_t  ext_irq;          /* bit 0 sound, bit 1 RPU, bit 2 timer */
    uint8_t  io_bank;
    uint8_t  scratch[4];
    int      scanline;
    int      hpos;
    int      raster_state;     /* 0 rendering, 1 hblank */
    bool     scanline_rendering;

    uint16_t l0_scroll_x, l0_scroll_y, l1_scroll_x, l1_scroll_y;

    struct {
        uint8_t cursor_x, cursor_y, save_x, save_y, scroll_x, scroll_y;
        uint8_t color, cursor_color, cursor_char, cursor_time;
        bool    auto_scroll_x, auto_scroll_y, wait_on_wrap, scroll_y_occurred;
        bool    wait_next_char, cursor_shown;
        uint8_t columns, lines;
        uint16_t cursor_addr;  /* tilemap offset under the cursor */
    } text;

    uint16_t palette[FZ_HW_PALETTE_SHOWN]; /* RGB565 */
    int      palette_latch;
    int      sprites_latch;
    int      sprites_on_screen;

    struct {
        uint32_t desc_addr;    /* physical */
        uint8_t  rd_cycles, wr_cycles;
        int      state;        /* DMA_IDLE .. DMA_RELEASE */
        uint32_t rd_addr, wr_addr;
        uint16_t remaining;
        uint8_t  flags;
        bool     bus_connected;
    } dma;

    struct {
        uint8_t  pc, state, upload_addr;
        bool     int_enabled, int_raw;
        uint16_t hmask, vmask;
        uint32_t instruction;
    } rpu;

    struct {
        bool     enabled, auto_reload, decrement, int_enabled, int_pending;
        uint16_t divider, reload, counter;
    } timer;

    uint32_t crc32;

    struct {
        uint8_t  clk_div, ram_len, operand;
        bool     busy, tf_cs;
        uint8_t  transfer_index, transfer_len;
        uint8_t  ram_rd_idx, ram_wr_idx;
        int      tf_state;     /* zvb_tf_state_t */
        uint8_t  tf_command;
        uint32_t tf_argument;
        uint64_t tf_size;      /* bytes, 0 when no card */
    } spi;
} fz_hw_video_t;

/* ------------------------------------------------------------------ */
/*  ZVB sound                                                          */
/* ------------------------------------------------------------------ */

#define FZ_HW_VOICES 4

typedef struct {
    struct {
        uint16_t freq_reg;     /* latched register value (pending reload) */
        uint16_t freq_active;  /* value currently played */
        double   freq_hz;      /* of freq_active */
        uint8_t  wave;         /* WAVE_SQUARE .. WAVE_NOISE */
        uint8_t  duty;         /* 0-7 (eighths, square only) */
        uint8_t  volume;       /* raw: bits 0-1 level, bit 7 mute */
        bool     hold, selected, left, right, pending_reload;
        uint16_t output;       /* current output level 0-65535 */
    } voice[FZ_HW_VOICES];

    uint8_t  enabled_reg, left_reg, right_reg, hold_reg, master_volume;

    struct {
        bool     selected, hold, routed_left, routed_right;
        uint8_t  divider, config;
        bool     is_u8, is_signed, loop, irq_enable, int_pending;
        bool     fifo_empty;
        int      fifo_count, fifo_head, fifo_tail;
        double   rate_hz;
        uint16_t output;
    } sample;

    int16_t last_left, last_right;
    int     sample_rate;
} fz_hw_sound_t;

/* ------------------------------------------------------------------ */
/*  I/O devices                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    struct {
        uint8_t mode, state, dir, int_vector, int_mask;
        bool    int_enable, and_op, active_high;
    } pio[2]; /* A (user port), B (system port) */

    struct {
        uint8_t tx_pos;
        uint8_t tx_bits;       /* bits shifted so far, LSB first */
        bool    tx_pin, rx_pin;
    } uart;

    struct {
        int     state;         /* i2c_state_t */
        uint8_t dev_addr, cur_byte;
        int     cur_bit;
        bool    has_reply, scl, sda_out, sda_in;
    } i2c;

    struct {
        uint8_t reg;
        int     count;
        uint8_t ram[8];        /* last registers read/written by the guest */
        int64_t offset_s;      /* programmed offset from host time */
        int     year, month, day, wday, hour, minute, second; /* live */
        bool    halted;        /* CH bit */
    } rtc;

    struct {
        uint16_t address;
        int      count;
        bool     writing;
        int      sector_written;
    } eeprom;

    struct {
        int     state;         /* ps2_state_t */
        uint8_t shift_register;
        bool    pin;
        int     fifo_count;
    } keyboard;

    struct {
        bool     present, failed, lba_mode, master;
        int      state;        /* compactflash_state_t */
        uint8_t  status, error, feature;
        int      sec_cnt, sec_cur, buffer_idx;
        uint32_t lba;
        uint64_t total_sectors;
    } cf;

    struct {
        uint8_t regs[16];
        bool    bound;
    } hostfs;

    struct {
        uint8_t     pages[4];
        const char* device[4];
    } mmu;
} fz_hw_io_t;

/** Fill a snapshot; false (and zeroed output) when no machine is bound. */
bool fz_hw_video(fz_hw_video_t* out);
bool fz_hw_sound(fz_hw_sound_t* out);
bool fz_hw_io(fz_hw_io_t* out);
/** Virtual address mapping a physical address through the current MMU
 *  pages, or -1 when it is not mapped. */
int32_t fz_hw_phys_to_virt(uint32_t phys);

#ifdef __cplusplus
}
#endif
