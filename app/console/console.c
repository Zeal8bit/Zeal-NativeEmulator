/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file console.c
 * @brief Interactive console for headless/automated runs.
 *
 * Reads one command per line from stdin and feeds keyboard events into the
 * existing keyboard device (key_pressed()/key_released()). The 'run' command
 * advances the emulation by an exact number of Z80 T-states, so test scripts
 * are deterministic and can run faster than real time.
 *
 * Debugger commands (see console_debug.c) are registered in the dispatch
 * table and are only available when the debugger is enabled (--debug).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/console/console.h"
#include "app/console/console_debug.h"
#include "app/console/console_keys.h"
#include "utils/helpers.h"
#include "utils/log.h"

#define CONSOLE_LINE_MAX 256
#define CONSOLE_MAX_ARGS 8
#if CONFIG_ENABLE_DEBUGGER
/* Debugger commands are grouped at the end of CONSOLE_COMMANDS[]; this is the
 * index of the first one. Used to gate them on the debugger being enabled.
 * Keep in sync with the non-debugger commands listed above. */
#define CONSOLE_FIRST_DEBUG_CMD 7
#endif

static void console_run_tstates(zeal_t* machine, int argc, char** argv)
{
    if (argc < 2) {
        log_err_printf("[CONSOLE] Usage: run <tstates>\n");
        return;
    }
    char* endptr = NULL;
    const unsigned long tstates = strtoul(argv[1], &endptr, 0);
    if (endptr == argv[1] || *endptr != '\0') {
        log_err_printf("[CONSOLE] Invalid tstates value: '%s'\n", argv[1]);
        return;
    }
    zeal_run_for_tstates(machine, tstates);
}


static void console_reset(zeal_t* machine, int argc, char** argv)
{
    (void) argc; (void) argv;
    zeal_reset(machine);
    log_printf("[CONSOLE] Reset\n");
}


static void console_quit(zeal_t* machine, int argc, char** argv)
{
    (void) argc; (void) argv;
    log_printf("[CONSOLE] Quit\n");
    zeal_exit(machine);
}


static void console_print_help(zeal_t* machine)
{
    log_printf("Zeal console commands:\n");
    log_printf("  help                  Show this help\n");
    log_printf("  press <key>           Press and hold a key\n");
    log_printf("  release <key>         Release a key\n");
    log_printf("  tap <key>             Press and release a key\n");
    log_printf("  run <tstates>         Advance the emulation by N Z80 T-states\n");
    log_printf("  reset                 Reset the emulator\n");
    log_printf("  quit                  Exit the emulator\n");
#if CONFIG_ENABLE_DEBUGGER
    if (machine->dbg_enabled) {
        log_printf("\nDebugger commands:\n");
        log_printf("  c, continue           Continue until breakpoint\n");
        log_printf("  s, step               Single step\n");
        log_printf("  so, step_over         Step over a call\n");
        log_printf("  p, pause              Pause\n");
        log_printf("  bp <addr|sym>         Set a breakpoint\n");
        log_printf("  bc <addr|sym>         Clear a breakpoint\n");
        log_printf("  bl                    List breakpoints\n");
        log_printf("  regs                  Show CPU registers\n");
        log_printf("  rb <addr> [count]     Read memory bytes\n");
        log_printf("  r16 <addr>            Read a 16-bit value\n");
        log_printf("  r32 <addr>            Read a 32-bit value\n");
        log_printf("  wb <addr> <v...>      Write memory bytes\n");
        log_printf("  u <addr> [count]      Disassemble memory\n");
        log_printf("  mmu                   Show MMU mappings\n");
    }
#endif
    log_printf("\nKeys: A-Z, 0-9, UP, DOWN, LEFT, RIGHT, ENTER, ESC, SPACE, BACKSPACE,\n");
    log_printf("      TAB, SHIFT, CTRL, ALT, HOME, END, DELETE, INSERT, PAGE_UP,\n");
    log_printf("      PAGE_DOWN, F1-F12, keypad and punctuation keys.\n");
}


static void console_help(zeal_t* machine, int argc, char** argv)
{
    (void) argc;
    (void) argv;
    console_print_help(machine);
}


/* Command dispatch table */
static const console_cmd_t CONSOLE_COMMANDS[] = {
    { "help",    console_help  },
    { "press",   console_key   },
    { "release", console_key   },
    { "tap",     console_tap   },
    { "run",     console_run_tstates },
    { "reset",   console_reset },
    { "quit",    console_quit  },
#if CONFIG_ENABLE_DEBUGGER
    { "continue", console_debug_continue },
    { "c",        console_debug_continue },
    { "step",     console_debug_step },
    { "s",        console_debug_step },
    { "step_over", console_debug_step_over },
    { "so",       console_debug_step_over },
    { "pause",    console_debug_pause },
    { "p",        console_debug_pause },
    { "bp",       console_debug_bp },
    { "bc",       console_debug_bc },
    { "bl",       console_debug_bl },
    { "regs",     console_debug_regs },
    { "rb",       console_debug_rb },
    { "r16",      console_debug_r16 },
    { "r32",      console_debug_r32 },
    { "wb",       console_debug_wb },
    { "u",        console_debug_disasm },
    { "mmu",      console_debug_mmu },
#endif
};


/* Split a line into whitespace-separated tokens; argv[0] is the command name.
 * Returns the number of tokens, at most max_args. */
static int console_tokenize(char* line, char** argv, int max_args)
{
    int argc = 0;
    char* p = line;

    while (*p != '\0' && argc < max_args) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    return argc;
}


static void console_exec_line(zeal_t* machine, char* line)
{
    /* Strip trailing newline / carriage return */
    char* end = line + strlen(line);
    while (end > line && (end[-1] == '\n' || end[-1] == '\r')) {
        end--;
    }
    *end = '\0';

    /* Skip leading whitespace */
    char* cmd = line;
    while (*cmd == ' ' || *cmd == '\t') {
        cmd++;
    }

    /* Empty line or comment */
    if (*cmd == '\0' || *cmd == '#') {
        return;
    }

    char* argv[CONSOLE_MAX_ARGS];
    const int argc = console_tokenize(line, argv, CONSOLE_MAX_ARGS);
    if (argc == 0) {
        return;
    }

    for (unsigned int i = 0; i < DIM(CONSOLE_COMMANDS); i++) {
        if (strcmp(argv[0], CONSOLE_COMMANDS[i].name) == 0) {
#if CONFIG_ENABLE_DEBUGGER
            /* Debugger commands are only available when the debugger is on */
            if (i >= CONSOLE_FIRST_DEBUG_CMD && !machine->dbg_enabled) {
                log_err_printf("[CONSOLE] Debugger not enabled (use --debug)\n");
                return;
            }
#endif
            CONSOLE_COMMANDS[i].handler(machine, argc, argv);
            return;
        }
    }

    log_err_printf("[CONSOLE] Unknown command: '%s' (type 'help' for a list)\n", argv[0]);
}


void console_run(zeal_t* machine)
{
    if (machine == NULL) {
        return;
    }

    char line[CONSOLE_LINE_MAX];

    while (!machine->should_exit) {
        log_printf("debug> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* End of input */
            break;
        }

        /* If the line was too long, drain the remainder so commands stay aligned */
        if (strlen(line) > 0 && line[strlen(line) - 1] != '\n' && !feof(stdin)) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {
            }
        }

        console_exec_line(machine, line);
    }
}
