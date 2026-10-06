/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "hw/zvb/zvb_text.h"


static void print_and_increment(zvb_text_t* text, uint8_t value, zvb_tilemap_t* tilemap);
static void cursor_next_line(zvb_text_t* text);

void zvb_text_init(zvb_text_t* text)
{
    assert(text != NULL);
    memset(text, 0, sizeof(*text));

    zvb_text_reset(text);
}


void zvb_text_reset(zvb_text_t* text)
{
    /* Set the default values */
    text->cursor_pos.raw  = 0;
    text->cursor_save.raw = 0;
    text->scroll.raw      = 0;
    text->wait_for_next_char = false;
    text->wait_for_next_char_save = false;
    text->color         = TEXT_DEFAULT_COLOR;
    text->cursor_color  = TEXT_DEFAULT_CURSOR_COLOR;
    text->cursor_time   = TEXT_DEFAULT_CURSOR_TIME;
    text->cursor_char   = TEXT_DEFAULT_CURSOR_CHAR;
    text->flags.val     = 0;
    text->frame_counter = 0;
    text->cursor_shown  = false;
    /* By default, we start in 80x40 mode */
    text->visible_lines   = TEXT_MAXIMUM_LINES;
    text->visible_columns = TEXT_MAXIMUM_COLUMNS;
}


void zvb_text_mode(zvb_text_t* text, bool col_80)
{
    if (col_80) {
        text->visible_lines   = TEXT_MAXIMUM_LINES;
        text->visible_columns = TEXT_MAXIMUM_COLUMNS;
    } else {
        text->visible_lines   = TEXT_MAXIMUM_LINES / 2;
        text->visible_columns = TEXT_MAXIMUM_COLUMNS / 2;
    }
}


void zvb_text_write(zvb_text_t* text, uint32_t addr, uint8_t value, zvb_tilemap_t* tilemap)
{
    switch(addr) {
        case TEXT_REG_PRINT_CHAR:
            text->flags.scroll_y_occurred = 0;
            print_and_increment(text, value, tilemap);
            break;

        case TEXT_REG_CURSOR_Y:
            if ((value & 0x3f) < text->visible_lines)
                text->cursor_pos.y = value & 0x3f;
            break;

        case TEXT_REG_CURSOR_X:
            if ((value & 0x7f) < text->visible_columns) {
                text->cursor_pos.x = value & 0x7f;
                text->wait_for_next_char = false;
            }
            break;

        case TEXT_REG_SCROLL_Y:
            text->scroll.y = (value & 0x3f) % TEXT_MAXIMUM_LINES;
            break;

        case TEXT_REG_SCROLL_X:
            text->scroll.x = (value & 0x7f) % TEXT_MAXIMUM_COLUMNS;
            break;

        case TEXT_REG_COLOR:
            text->color = value;
            break;

        case TEXT_REG_CURSOR_TIME:
            text->cursor_time = value;
            break;

        case TEXT_REG_CURSOR_CHAR:
            text->cursor_char = value;
            break;

        case TEXT_REG_CURSOR_COLOR:
            text->cursor_color = value;
            break;

        case TEXT_REG_CONTROL: {
            const zvb_pos_t previous_cursor = text->cursor_pos;
            const bool previous_wait = text->wait_for_next_char;
            text->flags.auto_scroll_y = (value >> 4) & 1;
            text->flags.wait_on_wrap  = (value >> 3) & 1;
            if ((value & TEXT_CTRL_RESTORE_CURSOR) != 0) {
                text->cursor_pos.raw = text->cursor_save.raw;
                text->wait_for_next_char = text->wait_for_next_char_save;
            }
            if ((value & TEXT_CTRL_SAVE_CURSOR) != 0) {
                text->cursor_save = previous_cursor;
                text->wait_for_next_char_save = previous_wait;
            }
            /* In the RTL this flag is set-only until reset. Enabling it also
             * brings a cursor awaiting wrap back onto the last column. */
            if ((value & 0x20) != 0) {
                text->flags.auto_scroll_x = 1;
                if (previous_wait) {
                    text->cursor_pos.x = (previous_cursor.x - 1) & 0x7f;
                    text->wait_for_next_char = false;
                }
            }
            if ((value & TEXT_CTRL_NEWLINE) != 0) {
                text->flags.scroll_y_occurred = 0;
                text->wait_for_next_char = false;
                text->cursor_pos.y = previous_cursor.y;
                cursor_next_line(text);
            }
            break;
        }

        default:
            break;
    }
}


uint8_t zvb_text_read(zvb_text_t* text, uint32_t addr)
{
    switch(addr) {
        case TEXT_REG_CURSOR_Y:
            return text->cursor_pos.y;
        case TEXT_REG_CURSOR_X:
            return text->cursor_pos.x;
        case TEXT_REG_SCROLL_Y:
            return text->scroll.y;
        case TEXT_REG_SCROLL_X:
            return text->scroll.x;
        case TEXT_REG_COLOR:
            return text->color;
        case TEXT_REG_CURSOR_TIME:
            return text->cursor_time;
        case TEXT_REG_CURSOR_CHAR:
            return text->cursor_char;
        case TEXT_REG_CURSOR_COLOR:
            return text->cursor_color;
        case TEXT_REG_CONTROL:
            return text->flags.val;
    }
    return 0;
}


bool zvb_text_update(zvb_text_t* text, zvb_text_info_t* info)
{
    if (text == NULL || info == NULL) {
        return false;
    }

    /* Check if we have to blink the cursor */
    if (text->cursor_time == 0 || text->cursor_time == 0xff) {
        text->cursor_shown = text->cursor_time == 0xff;
        text->frame_counter = 0;
    } else if (text->frame_counter >= text->cursor_time - 1) {
        text->cursor_shown = !text->cursor_shown;
        text->frame_counter = 0;
    } else {
        text->frame_counter++;
    }

    return zvb_text_get_info(text, info);
}

bool zvb_text_get_info(const zvb_text_t* text, zvb_text_info_t* info)
{
    if (text == NULL || info == NULL) return false;
    *info = (zvb_text_info_t) {
        .pos   = { text->cursor_pos.x, text->cursor_pos.y },
        .color = { (text->cursor_color >> 4) & 0xf,
                    text->cursor_color & 0xf },
        .charidx = text->cursor_char,
        .scroll = { text->scroll.x, text->scroll.y },
    };

    const bool shown = text->cursor_shown && !text->wait_for_next_char;
    if (!shown) {
        /* Hide the cursor by making the X coordinate out of bounds */
        info->pos[0] |= 0x80;
    }

    return shown;
}


static void print_and_increment(zvb_text_t* text, uint8_t value, zvb_tilemap_t* tilemap)
{
    /* Check if the cursor is pending (because of "eat-newline" feature) */
    if (text->wait_for_next_char) {
        text->wait_for_next_char = false;
        cursor_next_line(text);
    }

    const uint32_t index = zvb_text_cursor_address(text);
    zvb_tilemap_write(tilemap, 0, index, value);
    zvb_tilemap_write(tilemap, 1, index, text->color);

    /* Increment the cursor position */
    text->cursor_pos.x = (text->cursor_pos.x + 1) & 0x7f;
    if (text->cursor_pos.x == text->visible_columns) {
        /* Check if we have to scroll in X */
        if (text->flags.auto_scroll_x) {
            text->cursor_pos.x--;
            text->scroll.x = (text->scroll.x + 1) % TEXT_MAXIMUM_COLUMNS;
        } else if (text->flags.wait_on_wrap) {
            /* Check if we have to "eat newline", in other word, do we need to wait for a new character
                * before resetting X and updating Y. */
            text->wait_for_next_char = true;
        } else {
            cursor_next_line(text);
        }
    }
}


static void cursor_next_line(zvb_text_t* text)
{
    text->cursor_pos.x = 0;
    text->cursor_pos.y = (text->cursor_pos.y + 1) & 0x3f;

    /* Check if Y reached the bottom of the visible screen */
    if (text->cursor_pos.y == text->visible_lines) {
        if (text->flags.auto_scroll_y) {
            text->scroll.y = (text->scroll.y + 1) % TEXT_MAXIMUM_LINES;
            text->cursor_pos.y--;
            text->flags.scroll_y_occurred = 1;
        } else {
            text->cursor_pos.y = 0;
        }
    }
}
