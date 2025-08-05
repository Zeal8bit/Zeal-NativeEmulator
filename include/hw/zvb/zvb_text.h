/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hw/zvb/zvb_tilemap.h"

/**
 * @brief Text controller that can be mapped in the video board
 */
#define TEXT_DEFAULT_COLOR          0x0f
#define TEXT_DEFAULT_CURSOR_COLOR   0xf0
#define TEXT_DEFAULT_CURSOR_TIME    255
#define TEXT_DEFAULT_CURSOR_CHAR    0

#define TEXT_MAXIMUM_COLUMNS    80
#define TEXT_MAXIMUM_LINES      40

#define TEXT_CHAR_WIDTH         8
#define TEXT_CHAR_HEIGHT        12

/**
 * @brief I/O registers address, relative to the controller
 */
#define TEXT_REG_PRINT_CHAR     0
#define TEXT_REG_CURSOR_Y       1
#define TEXT_REG_CURSOR_X       2
#define TEXT_REG_SCROLL_Y       3
#define TEXT_REG_SCROLL_X       4
#define TEXT_REG_COLOR          5
#define TEXT_REG_CURSOR_TIME    6
#define TEXT_REG_CURSOR_CHAR    7
#define TEXT_REG_CURSOR_COLOR   8
#define TEXT_REG_CONTROL        9


/**
 * @brief Bitmasks for the flags
 */
#define TEXT_CTRL_NEWLINE           (1 << 0)
#define TEXT_CTRL_RESTORE_CURSOR    (1 << 6)
#define TEXT_CTRL_SAVE_CURSOR       (1 << 7)


/**
 * @brief Type for the 2D position of the cursor
 */
typedef union {
    struct {
        uint8_t x;
        uint8_t y;
    };
    uint16_t raw;
} zvb_pos_t;


/**
 * @brief Type for the text controller
 */
typedef struct {
    zvb_pos_t cursor_pos;
    zvb_pos_t cursor_save;
    zvb_pos_t scroll;
    uint8_t cursor_time;
    uint8_t cursor_char;
    uint8_t cursor_color;
    uint8_t color;
    union {
        struct {
            uint32_t scroll_y_occurred : 1;
            uint32_t rsv1 : 2;
            uint32_t wait_on_wrap : 1;
            uint32_t auto_scroll_y : 1;
            uint32_t auto_scroll_x : 1;
        };
        uint8_t val;
    } flags;

    /* Private fields */
    bool    wait_for_next_char;
    uint8_t visible_lines;
    uint8_t visible_columns;
    int     frame_counter;
    bool    cursor_shown;
} zvb_text_t;


/**
 * @brief Information about the cursor and scrolling
 */
typedef struct {
    int pos[2];
    int color[2];
    int charidx;
    int scroll[2];
} zvb_text_info_t;


/**
 * @brief Initialize the text, must be called before using it.
 */
void zvb_text_init(zvb_text_t* text);


/**
 * @brief Simulate a hardware reset on the text controller.
 */
void zvb_text_reset(zvb_text_t* text);


/**
 * @brief Switch text mode: swap between 80 and 40 columns modes
 */
void zvb_text_mode(zvb_text_t* text, bool col_80);


/**
 * @brief Function to call when a write occurs on the text I/O controller.
 *
 * @param layer Layer to write to (0 or 1)
 * @param addr Address relative to the text address space.
 * @param data Byte to write in the text
 */
void zvb_text_write(zvb_text_t* text, uint32_t addr, uint8_t value, zvb_tilemap_t* tilemap);


/**
 * @brief Function to call when a read occurs on the text I/O controller.
 *
 * @param layer Layer to read from (0 or 1)
 */
uint8_t zvb_text_read(zvb_text_t* text, uint32_t addr);


/**
 * @brief Update the text, must be called once per frame. The info structure will be filled.
 *
 * @returns true if the cursor is shown, false else.
 */
bool zvb_text_update(zvb_text_t* text, zvb_text_info_t* info);
