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
            text->cursor_pos.y = value;
            break;

        case TEXT_REG_CURSOR_X:
            text->cursor_pos.x = value;
            text->wait_for_next_char = false;
            break;

        case TEXT_REG_SCROLL_Y:
            text->scroll.y = value;
            break;

        case TEXT_REG_SCROLL_X:
            text->scroll.x = value;
            break;

        case TEXT_REG_COLOR:
            text->color = value;
            break;

        case TEXT_REG_CURSOR_TIME:
            text->cursor_time = value;
            text->frame_counter = 0;
            text->cursor_shown = false;
            break;

        case TEXT_REG_CURSOR_CHAR:
            text->cursor_char = value;
            break;

        case TEXT_REG_CURSOR_COLOR:
            text->cursor_color = value;
            break;

        case TEXT_REG_CONTROL:
            text->flags.auto_scroll_x = (value >> 5) & 1;
            text->flags.auto_scroll_y = (value >> 4) & 1;
            text->flags.wait_on_wrap  = (value >> 3) & 1;
            if ((value & TEXT_CTRL_SAVE_CURSOR) != 0 &&
                (value & TEXT_CTRL_RESTORE_CURSOR) != 0) {
                /* Exchange the cursor and the backup */
                uint16_t tmp = text->cursor_save.raw;
                text->cursor_save.raw = text->cursor_pos.raw;
                text->cursor_pos.raw = tmp;
            } else if ((value & TEXT_CTRL_SAVE_CURSOR) != 0) {
                text->cursor_save.raw = text->cursor_pos.raw;
            } else if ((value & TEXT_CTRL_RESTORE_CURSOR) != 0) {
                text->cursor_pos.raw = text->cursor_save.raw;
            }
            if ((value & TEXT_CTRL_NEWLINE) != 0) {
                text->flags.scroll_y_occurred = 0;
                text->wait_for_next_char = false;
                cursor_next_line(text);
            }
            break;

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
    if (text->cursor_time != 0 && text->cursor_time != 0xff) {
        if (++text->frame_counter == text->cursor_time) {
            text->cursor_shown = !text->cursor_shown;
            text->frame_counter = 0;
        }
    }

    *info = (zvb_text_info_t) {
        .pos   = { text->cursor_pos.x, text->cursor_pos.y },
        .color = { (text->cursor_color >> 4) & 0xf,
                    text->cursor_color & 0xf },
        .charidx = text->cursor_char,
        .scroll = { text->scroll.x, text->scroll.y },
    };

    if (!text->cursor_shown) {
        /* Hide the cursor by making the X coordinate out of bounds */
        info->pos[0] |= 0x80;
    }

    return text->cursor_shown;
}


static void print_and_increment(zvb_text_t* text, uint8_t value, zvb_tilemap_t* tilemap)
{
    /* Check if the cursor is pending (because of "eat-newline" feature) */
    if (text->wait_for_next_char) {
        text->wait_for_next_char = false;
        cursor_next_line(text);
    }

    /* Make sure the current cursor are not out of bounds */
    const uint32_t cursor_x = (text->cursor_pos.x + text->scroll.x) % text->visible_columns;
    const uint32_t cursor_y = (text->cursor_pos.y + text->scroll.y) % text->visible_lines;
    const uint32_t index = cursor_y * TEXT_MAXIMUM_COLUMNS + cursor_x;
    zvb_tilemap_write(tilemap, 0, index, value);
    zvb_tilemap_write(tilemap, 1, index, text->color);

    /* Increment the cursor position */
    text->cursor_pos.x++;
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
    text->cursor_pos.y++;

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
