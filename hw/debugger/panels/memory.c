/**
 * ===========================================================
 *                  MEMORY VIEWER
 * ===========================================================
 */

#include <stdio.h>
#include <ctype.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#define PANEL_FLAGS ( NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE )

#define ROWS        16   // Number of rows visible at a time
#define COLS        16   // Bytes per row
#define MEM_SIZE    256

#define BOTTOM_LINES    2
#define BOTTOM_HEIGHT   (BOTTOM_LINES) * 50

void ui_panel_memory(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel; // unreferenced
    struct nk_context* ctx = dctx->ctx;

    const float buttons_height = 85;
    struct nk_rect parent_bounds = nk_window_get_bounds(ctx);
    float scroll_height = parent_bounds.h - buttons_height;

    nk_layout_row_dynamic(ctx, scroll_height, 1);

    /* Scrollable region */
    if (nk_group_begin(ctx, "Memory_Scroll", NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 10, 1);

        char hex_left[25] = {0};
        char hex_right[25] = {0};
        char ascii[32] = {0};

        /* Retrieve the memory data */
        uint8_t mem[MEM_SIZE];
        debugger_read_memory(dbg, dctx->mem_view_addr, MEM_SIZE, mem);

        for (int i = 0; i < MEM_SIZE; i += COLS) {
            const int current_addr = dctx->mem_view_addr + i;
            /* Format hex values and ASCII characters */
            for (int j = 0; j < COLS; j++) {
                _Static_assert(MEM_SIZE % COLS == 0, "The memory dump size must be a multiple of columns");
                const uint8_t byte = mem[i + j];
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

    /* Add a line for the address to check and the dump */
    nk_layout_row_dynamic(ctx, 30, 3);
    static char edit_addr[64] = { 0 };
    nk_flags flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, edit_addr, sizeof(edit_addr), NULL);

    /**
     * Show a 'View' button to dump the memory
     * Parse the typed address if Enter was pressed or the buttonw as clicked
     */
    dbg_ui_mouse_hover(ctx);
    if ((nk_button_label(ctx, "View") || (flags & NK_EDIT_COMMITED))) {
        /* Make sure it is a hex value */
        hwaddr addr = 0;
        if (sscanf(edit_addr, "%x", &addr) == 1 || debugger_find_symbol(dbg, edit_addr, &addr)) {
            dctx->mem_view_addr = (int) addr;
        }
    }
}

