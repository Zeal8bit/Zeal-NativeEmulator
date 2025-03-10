#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"


#define WIN_UI_FONT_SIZE        13

#define CPU_CTRL_WIDTH          250
#define CPU_CTRL_HEIGHT         280
#define CPU_CTRL_REG_HEIGHT     25
#define CPU_CTRL_REG_PADDING    0.1f
#define DIM(t)  (sizeof(t) / sizeof(*t))
#define BIT(n)  (1 << (n))

struct dbg_ui_t {
    struct nk_context* ctx;
    struct nk_image    view;
    hwaddr             mem_view_addr;
    hwaddr             mem_view_size;
    hwaddr             dis_addr;
    hwaddr             dis_size;
};


static char BUFFER[256];

/**
 * ===========================================================
 *                  MENUBAR
 * ===========================================================
 */

#if 0
static void ui_menubar_setup(struct dbg_ui_t* dctx)
{
    struct nk_context* ctx = dctx->ctx;

    if (nk_begin(ctx, "Menubar", nk_rect(0, 0, 1280, 25),
                NK_WINDOW_NO_SCROLLBAR)) {

        nk_menubar_begin(ctx);
        /* menu #1 */
        nk_layout_row_begin(ctx, NK_STATIC, 25, 5);
        nk_layout_row_push(ctx, 45);
        if (nk_menu_begin_label(ctx, "MENU", NK_TEXT_LEFT, nk_vec2(120, 200)))
        {
            static size_t prog = 40;
            static int slider = 10;
            static bool check = nk_true;
            nk_layout_row_dynamic(ctx, 25, 1);
            if (nk_menu_item_label(ctx, "Hide", NK_TEXT_LEFT));
            if (nk_menu_item_label(ctx, "About", NK_TEXT_LEFT));
            nk_progress(ctx, &prog, 100, NK_MODIFIABLE);
            nk_slider_int(ctx, 0, &slider, 16, 1);
            nk_checkbox_label(ctx, "check", &check);
            nk_menu_end(ctx);
        }
        /* menu #2 */
        nk_layout_row_push(ctx, 60);
        if (nk_menu_begin_label(ctx, "ADVANCED", NK_TEXT_LEFT, nk_vec2(200, 600)))
        {
            enum menu_state {MENU_NONE,MENU_FILE, MENU_EDIT,MENU_VIEW,MENU_CHART};
            static enum menu_state menu_state = MENU_NONE;
            enum nk_collapse_states state;

            state = (menu_state == MENU_FILE) ? NK_MAXIMIZED: NK_MINIMIZED;
            if (nk_tree_state_push(ctx, NK_TREE_TAB, "FILE", &state)) {
                menu_state = MENU_FILE;
                nk_menu_item_label(ctx, "New", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Open", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Save", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Close", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Exit", NK_TEXT_LEFT);
                nk_tree_pop(ctx);
            } else menu_state = (menu_state == MENU_FILE) ? MENU_NONE: menu_state;

            state = (menu_state == MENU_EDIT) ? NK_MAXIMIZED: NK_MINIMIZED;
            if (nk_tree_state_push(ctx, NK_TREE_TAB, "EDIT", &state)) {
                menu_state = MENU_EDIT;
                nk_menu_item_label(ctx, "Copy", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Delete", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Cut", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Paste", NK_TEXT_LEFT);
                nk_tree_pop(ctx);
            } else menu_state = (menu_state == MENU_EDIT) ? MENU_NONE: menu_state;

            state = (menu_state == MENU_VIEW) ? NK_MAXIMIZED: NK_MINIMIZED;
            if (nk_tree_state_push(ctx, NK_TREE_TAB, "VIEW", &state)) {
                menu_state = MENU_VIEW;
                nk_menu_item_label(ctx, "About", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Options", NK_TEXT_LEFT);
                nk_menu_item_label(ctx, "Customize", NK_TEXT_LEFT);
                nk_tree_pop(ctx);
            } else menu_state = (menu_state == MENU_VIEW) ? MENU_NONE: menu_state;

            state = (menu_state == MENU_CHART) ? NK_MAXIMIZED: NK_MINIMIZED;
            if (nk_tree_state_push(ctx, NK_TREE_TAB, "CHART", &state)) {
                size_t i = 0;
                const float values[]={26.0f,13.0f,30.0f,15.0f,25.0f,10.0f,20.0f,40.0f,12.0f,8.0f,22.0f,28.0f};
                menu_state = MENU_CHART;
                nk_layout_row_dynamic(ctx, 150, 1);
                nk_chart_begin(ctx, NK_CHART_COLUMN, NK_LEN(values), 0, 50);
                for (i = 0; i < NK_LEN(values); ++i)
                    nk_chart_push(ctx, values[i]);
                nk_chart_end(ctx);
                nk_tree_pop(ctx);
            } else menu_state = (menu_state == MENU_CHART) ? MENU_NONE: menu_state;
            nk_menu_end(ctx);
        }
        nk_menubar_end(ctx);
    }
    nk_end(ctx);
}
#endif



/**
 * ===========================================================
 *                  HELPERS
 * ===========================================================
 */


static bool dbg_ui_clickable_label(struct nk_context* ctx, const char* label, const char* value)
{
    nk_label(ctx, label, NK_TEXT_LEFT);
    nk_style_push_color(ctx, &ctx->style.selectable.text_hover, nk_rgba(120, 120, 120, 255));
    bool ret = nk_select_label(ctx, value, NK_TEXT_LEFT, false);
    nk_style_pop_color(ctx);
    return ret;
}

static inline void dbg_ui_byte_to_hex(uint8_t byte, char* out, char separator) {
    static const char lut[] = "0123456789abcdef";
    out[0] = lut[byte >> 4];
    out[1] = lut[byte & 0x0F];
    if (separator != -1) {
        out[2] = separator;
    }
}

/**
 * ===========================================================
 *                  DISASSEMBLER VIEW
 * ===========================================================
 */

#define DISASSEMBLY_LINE_HEIGHT 15

static void ui_disassembler_viewer(struct dbg_ui_t* dctx, dbg_t* dbg)
{
    struct nk_context* ctx = dctx->ctx;
    const hwaddr pc = dctx->dis_addr;
    dbg_instr_t instr;

    /* Colors for breakpoints */
    const struct nk_color bps_bg_color    = nk_rgba(0xa4, 0, 0x0f, 0xff);
    // const struct nk_color bps_head_color  = nk_rgba(0xa4, 0, 0x0f, 0xff);
    const struct nk_color bps_hover_color = nk_rgba(0xbb, 0, 0x12, 0xff);

    /* Same for the current instruction */
    const struct nk_color cur_bg_color    = nk_rgba(0, 0x2d, 0x78, 0xff);
    // const struct nk_color cur_head_color  = nk_rgba(0, 0x2d, 0x78, 0xff);
    const struct nk_color cur_hover_color = nk_rgba(0, 0x45, 0xb8, 0xff);

    /* Color for regualr code */
    const struct nk_color reg_color   = nk_rgba(0xee, 0xee, 0xee, 0xff);

    if (nk_begin(ctx, "Disassembler", nk_rect(640 + CPU_CTRL_WIDTH, 0, 390, 950),
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_TITLE)) {

        /* Show a few instructions before PC */
        hwaddr current_addr = dctx->dis_addr; /*(dctx->dis_addr < 8) ? dctx->dis_addr : dctx->dis_addr - 8;*/

        for (size_t i = 0; i < dctx->dis_size; i++) {

            bool pushed = false;
            bool is_breakpoint = debugger_is_breakpoint_set(dbg, current_addr);

            /* Instruction Column, get teh current instruction */
            int instr_bytes = debugger_disassemble_address(dbg, current_addr, &instr);

            /* Check if there is a label */
            if (instr.label[0]) {
                nk_layout_row_dynamic(ctx, DISASSEMBLY_LINE_HEIGHT, 1);
                nk_label(ctx, instr.label, NK_TEXT_LEFT);
            }

            /* Generate an address-instruction pair line */
            nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
            nk_layout_row_push(ctx, 20);
            bool button_clicked = false;

            if (pc == current_addr) {
                pushed = true;
                button_clicked = nk_button_color(ctx, cur_bg_color);
                /* Change the background color for the current instruction */
                nk_style_push_color(ctx, &ctx->style.selectable.normal.data.color, cur_bg_color);
                nk_style_push_color(ctx, &ctx->style.selectable.hover.data.color, cur_hover_color);
            } else if (is_breakpoint) {
                /* Change the background for the breakpoints */
                pushed = true;
                button_clicked = nk_button_color(ctx, bps_bg_color);
                nk_style_push_color(ctx, &ctx->style.selectable.normal.data.color, bps_bg_color);
                nk_style_push_color(ctx, &ctx->style.selectable.hover.data.color, bps_hover_color);
            } else {
                button_clicked = nk_button_color(ctx, reg_color);
            }

            if (button_clicked) {
                /* Button was clicked, toggle the breakpoint! */
                if (is_breakpoint) debugger_clear_breakpoint(dbg, current_addr);
                else debugger_set_breakpoint(dbg, current_addr);
            }

            nk_layout_row_push(ctx, 370);

            snprintf(BUFFER, sizeof(BUFFER), "%04x:    %s",
                        current_addr,
                        instr.instruction);
            if (nk_select_label(ctx, BUFFER, NK_TEXT_LEFT, false)) {
                /* Label was clicked, do something ... if needed */
            }

            if (pushed) {
                /* If the background color was changed, restore the default one */
                nk_style_pop_color(ctx);
                nk_style_pop_color(ctx);
            }
            nk_layout_row_end(ctx);

            if (instr_bytes == -1) {
                break;
            }

            current_addr += instr_bytes;
        }
    }
    nk_end(ctx);
}


/**
 * ===========================================================
 *                  MEMORY VIEWER
 * ===========================================================
 */

#define ROWS        16   // Number of rows visible at a time
#define COLS        16   // Bytes per row
#define MEM_SIZE    256

#define BOTTOM_LINES    2
#define BOTTOM_HEIGHT   (BOTTOM_LINES) * 50


static void dbg_ui_go_to_mem(struct dbg_ui_t* dctx, hwaddr addr)
{
    dctx->mem_view_addr = addr;
}


static void ui_memory_viewer(struct dbg_ui_t* dctx, dbg_t* dbg)
{
    struct nk_context* ctx = dctx->ctx;

    struct nk_rect win_size = {0, 530, 640 + CPU_CTRL_WIDTH, 300};

    if (nk_begin(ctx, "Memory Viewer", win_size,
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(ctx, win_size.h - BOTTOM_HEIGHT, 1);

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
                snprintf(BUFFER, sizeof(BUFFER), "%04X:  %s| %s||  %s", current_addr, hex_left, hex_right, ascii);
                nk_label(ctx, BUFFER, NK_TEXT_LEFT);
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
        if ((nk_button_label(ctx, "View") || (flags & NK_EDIT_COMMITED))) {
            /* Make sure it is a hex value */
            hwaddr addr = 0;
            if (sscanf(edit_addr, "%x", &addr) == 1 || debugger_find_symbol(dbg, edit_addr, &addr)) {
                dctx->mem_view_addr = (int) addr;
            }
        }


    }
    nk_end(ctx);
}


/**
 * ===========================================================
 *                  CPU REGISTERS
 * ===========================================================
 */

#define REG_STR_LEN 6
#define REG_AF  0
#define REG_BC  1
#define REG_DE  2
#define REG_HL  3
#define REG_PC  4
#define REG_SP  5
#define REG_IX  6
#define REG_IY  7
#define REG_16_ONLY REG_PC

typedef struct {
    char     msb[3];
    char     lsb[3];
    char     pair[4];
    char     value[REG_STR_LEN];
    hwaddr   addr;
} regs_view_t;


static uint8_t* ui_idx_to_reg(regs_t* regs, int idx, bool lsb)
{
    switch (idx)
    {
        case REG_AF:
            if (lsb) return &regs->f;
            return &regs->a;
        case REG_BC:
            if (lsb) return &regs->c;
            return &regs->b;
        case REG_DE:
            if (lsb) return &regs->e;
            return &regs->d;
        case REG_HL:
            if (lsb) return &regs->l;
            return &regs->h;
        case REG_PC:
            return (uint8_t*) &regs->pc;
        case REG_SP:
            return (uint8_t*) &regs->sp;
        case REG_IX:
            return (uint8_t*) &regs->ix;
        case REG_IY:
            return (uint8_t*) &regs->iy;
        default:
            printf("ERROR INVALID REGISTER INDEX\n");
            return 0;
    }
}


static void ui_cpu_reg_edited(dbg_t* dbg, regs_t* regs, int idx, bool lsb, char* value)
{
    uint8_t* dst = ui_idx_to_reg(regs, idx, lsb);
    /* Convert the hex value to binary */
    *dst = (uint8_t) strtol(value, NULL, 16);
    debugger_set_registers(dbg, regs);
}



static void ui_cpu_ctrl(struct dbg_ui_t* dctx, dbg_t* dbg)
{
    struct nk_context* ctx = dctx->ctx;
    regs_t regs = { 0 };

    regs_view_t pairs[] = {
        [REG_AF] = {
            .msb  = "A:",  .lsb  = "F:",
            .pair = "AF:", .value = "     ",
        },
        [REG_BC] = {
            .msb  = "B:",  .lsb  = "C:",
            .pair = "BC:", .value = "     ",
        },
        [REG_DE] = {
            .msb  = "D:",  .lsb  = "E:",
            .pair = "DE:", .value = "     ",
        },
        [REG_HL] = {
            .msb  = "H:",  .lsb  = "L:",
            .pair = "HL:", .value = "     ",
        },
        [REG_PC] = { .pair = "PC:",  .value = "     ", },
        [REG_SP] = { .pair = "SP:",  .value = "     ", },
        [REG_IX] = { .pair = "IX:",  .value = "     ", },
        [REG_IY] = { .pair = "IY:",  .value = "     ", },
    };

    /* When the target is shown, we can show the values */
    const bool paused = debugger_is_paused(dbg);
    if (paused) {
        debugger_get_registers(dbg, &regs);
        snprintf(pairs[REG_AF].value, REG_STR_LEN, "$%04x", regs.af);
        snprintf(pairs[REG_BC].value, REG_STR_LEN, "$%04x", regs.bc);
        snprintf(pairs[REG_DE].value, REG_STR_LEN, "$%04x", regs.de);
        snprintf(pairs[REG_HL].value, REG_STR_LEN, "$%04x", regs.hl);
        snprintf(pairs[REG_PC].value, REG_STR_LEN, "$%04x", regs.pc);
        snprintf(pairs[REG_SP].value, REG_STR_LEN, "$%04x", regs.sp);
        snprintf(pairs[REG_IX].value, REG_STR_LEN, "$%04x", regs.ix);
        snprintf(pairs[REG_IY].value, REG_STR_LEN, "$%04x", regs.iy);
        pairs[REG_AF].addr = regs.af;
        pairs[REG_BC].addr = regs.bc;
        pairs[REG_DE].addr = regs.de;
        pairs[REG_HL].addr = regs.hl;
        pairs[REG_PC].addr = regs.pc;
        pairs[REG_SP].addr = regs.sp;
        pairs[REG_IX].addr = regs.ix;
        pairs[REG_IY].addr = regs.iy;

        /* Save the current PC for the disassembler view */
        dctx->dis_addr = regs.pc;
    }


    if (nk_begin(ctx, "CPU Control", nk_rect(640, 0, CPU_CTRL_WIDTH, CPU_CTRL_HEIGHT),
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
        NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
    {
        static struct {
            size_t index;
            bool lsb;
            char value[3];
        } editing = { .index = REG_16_ONLY };

        size_t i;
        for (i = 0; i < REG_16_ONLY; i++) {
            regs_view_t* reg = &pairs[i];

            const float ratio[] = { 0.1f, 0.15f,
                                    0.1f, 0.15f,
                                    0.25f, 0.25f };
            nk_layout_row(ctx, NK_DYNAMIC, CPU_CTRL_REG_HEIGHT, 6, ratio);

            // nk_layout_row_dynamic(ctx, CPU_CTRL_REG_HEIGHT, 6);
            /* Create a row of 3 elements (3 pairs of label + input) */
            nk_label(ctx, reg->msb, NK_TEXT_LEFT);
            if (editing.index == i && !editing.lsb && paused) {
                /* If the signal is sent, enter was pressed */
                if (nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                                                   editing.value, 3, nk_filter_hex) & NK_EDIT_COMMITED)
                {
                    /* Enter key was pressed */
                    ui_cpu_reg_edited(dbg, &regs, i, false, editing.value);
                    /* Hide the editing box */
                    editing.index = REG_16_ONLY;
                }
            } else if (nk_select_text(ctx, reg->value + 1, 2, NK_TEXT_LEFT, false) && paused) {
                /* The label was clicked when the emulation was paused. Turn it into an editable box */
                editing.index = i;
                editing.lsb = false;
                editing.value[0] = reg->value[1];
                editing.value[1] = reg->value[2];
            }

            nk_label(ctx, reg->lsb, NK_TEXT_LEFT);
            if (editing.index == i && editing.lsb && paused) {
                /* If the signal is sent, enter was pressed */
                if (nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER,
                                                   editing.value, 3, nk_filter_hex) & NK_EDIT_COMMITED)
                {
                    /* Enter key was pressed */
                    ui_cpu_reg_edited(dbg, &regs, i, true, editing.value);
                    /* Hide the editing box */
                    editing.index = REG_16_ONLY;
                }
            } else if (nk_select_text(ctx, reg->value + 3, 2, NK_TEXT_LEFT, false) && paused) {
                /* The label was clicked! Turn it into an editable box */
                editing.index = i;
                editing.lsb = true;
                editing.value[0] = reg->value[3];
                editing.value[1] = reg->value[4];
            }

            /* Only accept clickable label if paused */
            if (dbg_ui_clickable_label(ctx, reg->pair, reg->value) && paused) {
                dbg_ui_go_to_mem(dctx, reg->addr);
            }
        }

        /* Display all the registers that are 16-bit only */
        nk_layout_row_dynamic(ctx, CPU_CTRL_REG_HEIGHT, 4);
        for (i = REG_16_ONLY; i < DIM(pairs); i++) {
            regs_view_t* reg = &pairs[i];
            if (dbg_ui_clickable_label(ctx, reg->pair, reg->value) && paused) {
                dbg_ui_go_to_mem(dctx, reg->addr);
            }
        }

        /* Flags related */
        nk_layout_row_dynamic(ctx, CPU_CTRL_REG_HEIGHT, 6);
        nk_check_label(ctx, "C",   (regs.f & BIT(0)) != 0);
        nk_check_label(ctx, "N",   (regs.f & BIT(1)) != 0);
        nk_check_label(ctx, "P/V", (regs.f & BIT(2)) != 0);
        nk_check_label(ctx, "H",   (regs.f & BIT(4)) != 0);
        nk_check_label(ctx, "Z",   (regs.f & BIT(6)) != 0);
        nk_check_label(ctx, "S",   (regs.f & BIT(7)) != 0);

        nk_layout_row_dynamic(ctx, CPU_CTRL_REG_HEIGHT, 4);

        if (paused) {
            /* Machine is paused, show the continue button */
            if (nk_button_label(ctx, ">"))
                debugger_continue(dbg);
        } else {
            /* Machine is running, show the pause button */
            if (nk_button_label(ctx, "||"))
                debugger_pause(dbg);
        }

        if (nk_button_label(ctx, ">|")) {
            debugger_step(dbg);
        }


        if (nk_button_label(ctx, ">>|")) {
            printf("Step Over pressed\n");
        }

        if (nk_button_label(ctx, "[ ]")) {
            printf("Restart pressed\n");
        }
    }
    nk_end(ctx);
}


/**
 * ===========================================================
 *                  BREAKPOINTS
 * ===========================================================
 */

static void ui_breakpoints(struct dbg_ui_t* dctx, dbg_t* dbg)
{
    struct nk_context* ctx = dctx->ctx;
    /* Input buffer used for adding a new breakpoint, it must be kept alive
     * through all the callsm, so put it in static. */
    static char input[32] = { 0 };
    hwaddr new_bp;

    /* Get the breakpoints from the debugger core */
    hwaddr breakpoints[DBG_MAX_POINTS];
    int bp_len = debugger_get_breakpoints(dbg, breakpoints, DBG_MAX_POINTS);

    if (nk_begin(ctx, "Breakpoints", nk_rect(640, CPU_CTRL_HEIGHT, CPU_CTRL_WIDTH, CPU_CTRL_HEIGHT - 30),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
    {
        /* Height of the field at the bottom that lets the user add a breakpoint */
        const float add_field_height = 85;
        struct nk_rect parent_bounds = nk_window_get_bounds(ctx); // Get window size
        float scroll_height = parent_bounds.h - add_field_height;
        nk_layout_row_dynamic(ctx, scroll_height, 1); // This ensures the scroll area takes space

        /* Create a new group to have a scroll section */
        if (nk_group_begin(ctx, "BreakpointsList", NK_WINDOW_BORDER)) {

            for (int i = 0; i < bp_len; i++) {

                /* Let's make a custom row to have a smaller 'x' button */
                nk_layout_row_begin(ctx, NK_STATIC, 25, 2);
                nk_layout_row_push(ctx, parent_bounds.w - 80);

                snprintf(BUFFER, sizeof(BUFFER), "0x%04X", breakpoints[i]);
                nk_label(ctx, BUFFER, NK_TEXT_LEFT);


                nk_layout_row_push(ctx, 30);
                if (nk_button_label(ctx, "x")) {
                    debugger_clear_breakpoint(dbg, breakpoints[i]);
                }
                nk_layout_row_end(ctx);
            }

            nk_group_end(ctx);
        }

        /* Fixed input row at the bottom */
        nk_layout_row_dynamic(ctx, 30, 2);
        nk_flags flags = nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD | NK_EDIT_SIG_ENTER, input, 32, NULL);

        /* Show an 'Add' button to set a breakpoint */
        if (nk_button_label(ctx, "Add") || (flags & NK_EDIT_COMMITED)) {
            /* Make sure it is a hex value (for now) */
            if (sscanf(input, "%x", &new_bp) == 1 || debugger_find_symbol(dbg, input, &new_bp)) {
                /* Clear the buffer to clear the field */
                memset(input, 0, sizeof(input));
                debugger_set_breakpoint(dbg, new_bp);
            }
        }


    }
    nk_end(ctx);
}


/**
 * ===========================================================
 *                  ZEAL VIEW
 * ===========================================================
 */


static void ui_main_view(struct dbg_ui_t* dctx)
{
    struct nk_context* ctx = dctx->ctx;
    struct nk_image* img = &dctx->view;

    if(nk_begin(ctx, "Zeal Video output", nk_rect(0, 0, img->w, img->h + 50),
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_MOVABLE))
    {
        nk_layout_row_static(ctx, img->h, img->w, 1);
        nk_image(ctx, *img);
    }
    nk_end(ctx);
}

/**
 * ===========================================================
 *                  PUBLIC INTERFACE
 * ===========================================================
 */

int debugger_ui_init(struct dbg_ui_t** ret_ctx, RenderTexture2D* emu_view)
{
    if (ret_ctx == NULL) {
        return 0;
    }

    struct dbg_ui_t* dbg_ctx = MemAlloc(sizeof(struct dbg_ui_t));
    if (dbg_ctx == NULL) {
        return 0;
    }

    Font font = LoadFontFromNuklear(WIN_UI_FONT_SIZE);
    struct nk_context* ctx = InitNuklearEx(font, WIN_UI_FONT_SIZE);
    if (ctx == NULL) {
        MemFree(dbg_ctx);
        return 0;
    }
    dbg_ctx->ctx = ctx;
    SetNuklearScaling(ctx, 1);

    /* Convert the RayLib texture into a Nuklear image */
    dbg_ctx->view = TextureToNuklear(emu_view->texture);

    /* Set the default attributes */
    dbg_ctx->mem_view_size = 256;
    dbg_ctx->mem_view_addr = 0;
    dbg_ctx->dis_addr = 0;
    dbg_ctx->dis_size = 50;

    *ret_ctx = dbg_ctx;
    return 1;
}


void debugger_ui_deinit(struct dbg_ui_t* ctx)
{
    UnloadNuklearImage(ctx->view);
    UnloadNuklear(ctx->ctx);
    MemFree(ctx);
}


void debugger_ui_prepare_render(struct dbg_ui_t* ctx, dbg_t* dbg)
{
    UpdateNuklear(ctx->ctx);
    ui_main_view(ctx);
    ui_cpu_ctrl(ctx, dbg);
    ui_breakpoints(ctx, dbg);
    ui_memory_viewer(ctx, dbg);
    ui_disassembler_viewer(ctx, dbg);
    // ui_menubar_setup(ctx);
}


void debugger_ui_render(struct dbg_ui_t* ctx, dbg_t* dbg)
{
    (void) dbg;
    DrawNuklear(ctx->ctx);
}


bool debugger_ui_main_view_focused(const struct dbg_ui_t* ctx)
{
    return nk_window_is_active(ctx->ctx, "Zeal Video output");
}
