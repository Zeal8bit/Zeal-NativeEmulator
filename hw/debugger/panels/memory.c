/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * ===========================================================
 *                  MEMORY VIEWER
 * ===========================================================
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"
#include "utils/log.h"
#include "utils/helpers.h"

#define PANEL_FLAGS ( NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE )

#define ROWS        16   // Number of rows visible at a time
#define COLS        16   // Bytes per row

#define TABS_MAX_COUNT  16

#define BOTTOM_LINES    2
#define BOTTOM_HEIGHT   (BOTTOM_LINES) * 50

#define DEFAULT_MEM_SIZE    256

#define SIZE_128     0
#define SIZE_256     1
#define SIZE_512     2
#define SIZE_1024    3

static const char* size_names[] = {
    [SIZE_128] = "128",
    [SIZE_256] = "256",
    [SIZE_512] = "512",
    [SIZE_1024] = "1024",
};

static int size_values[] = {
    [SIZE_128] = 128,
    [SIZE_256] = 256,
    [SIZE_512] = 512,
    [SIZE_1024] = 1024,
};

/* Each tab has its own address range and bytes count */
typedef struct {
    int addr;
    int size;
    int size_index;
    uint8_t* mem;
} tab_t;

static tab_t tabs_addr[TABS_MAX_COUNT] = { 0 };
static int current_tab = 0;

/* Define the colors for the tabs */
static const struct nk_color s_active_color   = { .r = 0, .g = 0x2d, .b = 0x78, .a = 0xff };
static const struct nk_color s_inactive_color = { .r = 0x6c, .g = 0x70, .b = 0x86, .a = 0xff };

/* At all time, we have one tab opened, and another one that has a `+`. When the user clicks on the `+`, a new tab is opened */
static int tabs_count = 1;

static void create_new_tab(tab_t* tab)
{
    tab->addr = 0;
    tab->size = size_values[SIZE_256];
    tab->size_index = SIZE_256;
    tab->mem = malloc(size_values[SIZE_256]);
    if (!tab->mem) {
        log_perror("[MEMORY] Failed to allocate memory for new tab");
    }
}

/* Initialize the first tab, check `mem` to know whether we need to initialize it or not */
static void init_first_tab(void)
{
    if (!tabs_addr[0].mem) {
        create_new_tab(&tabs_addr[0]);
    }
}

static void render_memory_content(struct nk_context* ctx, dbg_t* dbg, tab_t* tab, int tab_index)
{
    /* Scrollable region, each tab refers to a different memory address range, call the dump function for each tab */
    char group_name[32];
    snprintf(group_name, sizeof(group_name), "Memory_Scroll_%d", tab_index);
    if (nk_group_begin(ctx, group_name, NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 10, 1);

        char hex_left[25] = {0};
        char hex_right[25] = {0};
        char ascii[32] = {0};

        /* Retrieve the memory data */
        debugger_read_memory(dbg, tab->addr, tab->size, tab->mem);

        for (int i = 0; i < tab->size; i += COLS) {
            const int current_addr = tab->addr + i;
            /* Format hex values and ASCII characters */
            for (int j = 0; j < COLS; j++) {
                const uint8_t byte = tab->mem[i + j];
                int index = (j % 8) * 3;
                char* dst = (j < 8) ? &hex_left[index] : &hex_right[index];
                dbg_ui_byte_to_hex(byte, dst, ' ');
                /* Check if the character is printable */
                ascii[j] = isprint((char) byte) ? byte : '.';
            }
            /* The three buffers are already NULL-terminated */

            /* Build the final line. TODO: Add memory type in front of the address. */
            snprintf(DEBUG_BUFFER, sizeof(DEBUG_BUFFER), "%04X:  %s| %s||  %s", current_addr, hex_left, hex_right, ascii);
            nk_label(ctx, DEBUG_BUFFER, NK_TEXT_LEFT);
        }

        nk_group_end(ctx);
    }
}

static void render_bottom_controls(struct nk_context* ctx, dbg_t* dbg, tab_t* tab)
{
    /* Single line with address input, View button, and Size dropdown */
    /* Use a template: address field (flexible), button (fixed), dropdown (fixed) */
    float button_width = 60.0f;
    float dropdown_width = 80.0f;
    nk_layout_row_template_begin(ctx, 30);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, button_width);
    nk_layout_row_template_push_static(ctx, dropdown_width);
    nk_layout_row_template_end(ctx);

    static char edit_addr[64] = { 0 };
    dbg_ui_mouse_hover(ctx, MOUSE_TEXT);
    nk_flags flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, edit_addr, sizeof(edit_addr), NULL);

    /**
     * Show a 'View' button to dump the memory
     * Parse the typed address if Enter was pressed or the button was clicked
     */
    dbg_ui_mouse_hover(ctx, MOUSE_POINTER);
    if ((nk_button_label(ctx, "View") || (flags & NK_EDIT_COMMITED))) {
        /* Make sure it is a hex value */
        hwaddr addr = 0;
        if (sscanf(edit_addr, "%x", &addr) == 1 || debugger_find_symbol(dbg, edit_addr, &addr)) {
            tab->addr = (int) addr;
        }
    }

    /* Show a `Size` dropdown to choose the size of the memory to dump */
    if (nk_combo_begin_label(ctx, size_names[tab->size_index], nk_vec2(dropdown_width, 200))) {
        nk_layout_row_dynamic(ctx, 25, 1);
        for (int i = 0; i < (int) DIM(size_names); i++) {
            if (nk_combo_item_label(ctx, size_names[i], NK_TEXT_LEFT)) {
                int new_size = size_values[i];
                if (new_size != tab->size) {
                    uint8_t* new_mem = realloc(tab->mem, new_size);
                    if (new_mem) {
                        tab->mem = new_mem;
                        tab->size = new_size;
                        tab->size_index = i;
                    } else {
                        log_perror("[MEMORY] Failed to reallocate memory for tab");
                    }
                } else {
                    tab->size_index = i;
                }
            }
        }
        nk_combo_end(ctx);
    }
}

void ui_panel_memory(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel;
    struct nk_context* ctx = dctx->ctx;

    init_first_tab();

    const float tab_row_height = 30.0f;
    const float bottom_controls_height = 30.0f;
    const float padding = 8.0f; /* Account for spacing between layout rows and borders */
    struct nk_vec2 content_size = nk_window_get_content_region_size(ctx);
    float scroll_height = content_size.y - tab_row_height - bottom_controls_height - padding;

    /* Display the tabs row with fixed width buttons */
    const float tab_button_width = 80.0f;
    const float add_button_width = 30.0f;

    /* Use template layout for fixed-width tabs */
    nk_layout_row_template_begin(ctx, tab_row_height);
    for (int i = 0; i < tabs_count; ++i) {
        nk_layout_row_template_push_static(ctx, tab_button_width);
    }
    if (tabs_count < TABS_MAX_COUNT) {
        nk_layout_row_template_push_static(ctx, add_button_width);
    }
    nk_layout_row_template_end(ctx);

    /* Render tab buttons */
    for (int i = 0; i < tabs_count; ++i) {
        if (i == current_tab) {
            nk_style_push_color(ctx, &ctx->style.button.normal.data.color, s_active_color);
        } else {
            nk_style_push_color(ctx, &ctx->style.button.normal.data.color, s_inactive_color);
        }

        char tab_label[16];
        snprintf(tab_label, sizeof(tab_label), "%04X", tabs_addr[i].addr);
        if (nk_button_label(ctx, tab_label)) {
            current_tab = i;
        }

        nk_style_pop_color(ctx);
    }

    /* Show the '+' button to add new tabs */
    if (tabs_count < TABS_MAX_COUNT) {
        if (nk_button_label(ctx, "+")) {
            current_tab = tabs_count;
            create_new_tab(&tabs_addr[current_tab]);
            tabs_count++;
        }
    }

    /* Render the scrollable memory content of the current tab */
    nk_layout_row_dynamic(ctx, scroll_height, 1);
    render_memory_content(ctx, dbg, &tabs_addr[current_tab], current_tab);

    /* Render the bottom controls (always visible, not scrollable) */
    render_bottom_controls(ctx, dbg, &tabs_addr[current_tab]);
}

