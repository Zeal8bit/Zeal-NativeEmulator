/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include "frontend/common/app.h"
#include "frontend/common/cli.h"
#include "frontend/common/desktop.h"
#include "frontend/headless/console.h"
#include "utils/log.h"

static const char skill[] =
#include "frontend/headless/skill.inc"
;

static bool console;
static unsigned long run_ticks;

int frontend_run(zeal_t* machine)
{
    if (console) {
        console_run(machine);
    } else {
        zeal_run(machine, run_ticks);
        if (run_ticks > 0 && machine->cpu.cyc >= run_ticks) {
            log_printf("[ZEAL] Ran for %lu ticks\n", machine->cpu.cyc);
        }
    }
    return machine->compactflash.failed ? 1 : 0;
}

int main(int argc, char* argv[])
{
    bool print_skill = false;
    bool debug_console = false;
    const frontend_cli_option_t options[] = {
        {'S', "skill", "  -S, --skill                        Print the embedded agent SKILL.md and exit",
         .flag = &print_skill},
        {'g', "debug", "  -g, --debug                        Alias for --console",
         .flag = &debug_console, .optional_bool = true},
        {'n', "headless", "  -n, --headless [<tstates>]         Run without a GUI, optionally for a fixed number of tstates",
         .number = &run_ticks},
        {'o', "console", "  -o, --console                      Read control commands from stdin",
         .flag = &console},
    };
    int code = frontend_configure(argc, argv, options, sizeof(options) / sizeof(options[0]));
    if (code) {
        return code;
    }
    if (print_skill) {
#ifdef _WIN32
        if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
            return 1;
        }
#endif
        return fputs(skill, stdout) == EOF || fflush(stdout) == EOF;
    }
    if (debug_console) {
        console = true;
    }
    v_log_printf(2, "[CONFIG] headless_run_ticks: %lu\n", run_ticks);
    v_log_printf(2, "[CONFIG] console: %s\n", console ? "True" : "False");
    return desktop_run(false);
}
