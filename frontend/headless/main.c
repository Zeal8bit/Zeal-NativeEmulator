/* SPDX-License-Identifier: Apache-2.0 */
#include "frontend/common/app.h"
#include "frontend/common/cli.h"
#include "frontend/common/desktop.h"
#include "frontend/headless/console.h"
#include "utils/log.h"

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
    const frontend_cli_option_t options[] = {
        {'n', "headless", "  -n, --headless [<tstates>]         Run without a GUI, optionally for a fixed number of tstates",
         .number = &run_ticks},
        {'o', "console", "  -o, --console                      Read control commands from stdin",
         .flag = &console},
    };
    int code = frontend_configure(argc, argv, options, sizeof(options) / sizeof(options[0]));
    if (code) return code;
    bool debug = config.debugger.enabled == DEBUGGER_STATE_ARG;
    if (debug) console = true;
    v_log_printf(2, "[CONFIG] headless_run_ticks: %lu\n", run_ticks);
    v_log_printf(2, "[CONFIG] console: %s\n", console ? "True" : "False");
    return desktop_run(false, debug);
}
