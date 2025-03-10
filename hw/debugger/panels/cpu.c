/**
 * ===========================================================
 *                  CPU REGISTERS
 * ===========================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/helpers.h"
#include "ui/raylib-nuklear.h"
#include "debugger/debugger.h"
#include "debugger/debugger_ui.h"

#define REG_STR_LEN 6
#define REG_AF      0
#define REG_BC      1
#define REG_DE      2
#define REG_HL      3
#define REG_PC      4
#define REG_SP      5
#define REG_IX      6
#define REG_IY      7
#define REG_16_ONLY REG_PC

#define PANEL_FLAGS ( NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE )

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



void ui_panel_cpu(struct dbg_ui_panel_t* panel, struct dbg_ui_t* dctx, dbg_t* dbg)
{
    (void)panel; // unreferenced
    struct nk_context* ctx = dctx->ctx;
    regs_t regs = { 0 };


    regs_view_t pairs[] = {
        [REG_AF] = {
            .msb  = "A:",  .lsb  = "F:",
            .pair = "AF:", .value = "      ",
        },
        [REG_BC] = {
            .msb  = "B:",  .lsb  = "C:",
            .pair = "BC:", .value = "      ",
        },
        [REG_DE] = {
            .msb  = "D:",  .lsb  = "E:",
            .pair = "DE:", .value = "      ",
        },
        [REG_HL] = {
            .msb  = "H:",  .lsb  = "L:",
            .pair = "HL:", .value = "      ",
        },
        [REG_PC] = { .pair = "PC:",  .value = "      ", },
        [REG_SP] = { .pair = "SP:",  .value = "      ", },
        [REG_IX] = { .pair = "IX:",  .value = "      ", },
        [REG_IY] = { .pair = "IY:",  .value = "      ", },
    };

    /* When the target is shown, we can show the values */
    const bool paused = debugger_is_paused(dbg);
    if (paused) {
        debugger_get_registers(dbg, &regs);


        snprintf(pairs[REG_AF].value, REG_STR_LEN, "$    ");
        snprintf(pairs[REG_BC].value, REG_STR_LEN, "$    ");
        snprintf(pairs[REG_DE].value, REG_STR_LEN, "$    ");
        snprintf(pairs[REG_HL].value, REG_STR_LEN, "$    ");
        snprintf(pairs[REG_PC].value, REG_STR_LEN, "$    ");
        snprintf(pairs[REG_SP].value, REG_STR_LEN, "$    ");
        snprintf(pairs[REG_IX].value, REG_STR_LEN, "$    ");
        snprintf(pairs[REG_IY].value, REG_STR_LEN, "$    ");

        dbg_ui_word_to_hex(regs.bc, &pairs[REG_BC].value[1], -1);
        dbg_ui_word_to_hex(regs.de, &pairs[REG_DE].value[1], -1);
        dbg_ui_word_to_hex(regs.af, &pairs[REG_AF].value[1], -1);
        dbg_ui_word_to_hex(regs.hl, &pairs[REG_HL].value[1], -1);
        dbg_ui_word_to_hex(regs.pc, &pairs[REG_PC].value[1], -1);
        dbg_ui_word_to_hex(regs.sp, &pairs[REG_SP].value[1], -1);
        dbg_ui_word_to_hex(regs.ix, &pairs[REG_IX].value[1], -1);
        dbg_ui_word_to_hex(regs.iy, &pairs[REG_IY].value[1], -1);

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
    nk_check_label(ctx, "C",   BIT(regs.f, 0) != 0);
    nk_check_label(ctx, "N",   BIT(regs.f, 1) != 0);
    nk_check_label(ctx, "P/V", BIT(regs.f, 2) != 0);
    nk_check_label(ctx, "H",   BIT(regs.f, 4) != 0);
    nk_check_label(ctx, "Z",   BIT(regs.f, 6) != 0);
    nk_check_label(ctx, "S",   BIT(regs.f, 7) != 0);


    nk_layout_row_dynamic(ctx, CPU_CTRL_REG_HEIGHT, 4);

    if (paused) {
        /* Machine is paused, show the continue button */
        dbg_ui_mouse_hover(ctx);
        if (nk_button_label(ctx, ">"))
            debugger_continue(dbg);
    } else {
        /* Machine is running, show the pause button */
        if (nk_button_label(ctx, "||"))
            debugger_pause(dbg);
    }

    dbg_ui_mouse_hover(ctx);
    if (nk_button_label(ctx, ">|")) {
        debugger_step(dbg);
    }

    dbg_ui_mouse_hover(ctx);
    if (nk_button_label(ctx, ">>|")) {
        debugger_step_over(dbg);
    }

    dbg_ui_mouse_hover(ctx);
    if (nk_button_label(ctx, "[ ]")) {
        debugger_restart(dbg);
    }
}
