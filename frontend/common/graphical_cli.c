/* SPDX-License-Identifier: Apache-2.0 */
#include "frontend/common/cli.h"
#include "utils/log.h"

int frontend_configure_graphical(int argc, char* argv[], const frontend_cli_option_t* options, size_t count)
{
    bool redirected = false;
    unsigned long ignored_ticks = 0;
    frontend_cli_option_t all[count + 2];
    for (size_t i = 0; i < count; i++) all[i] = options[i];
    all[count] = (frontend_cli_option_t){
        'n', "headless", "  -n, --headless                     Moved to zeal-headless",
        .flag = &redirected, .number = &ignored_ticks
    };
    all[count + 1] = (frontend_cli_option_t){
        'o', "console", "  -o, --console                      Moved to zeal-headless", .flag = &redirected
    };

    int code = frontend_configure(argc, argv, all, count + 2);
    if (code) return code;
    if (redirected) {
        log_err_printf("Headless and console modes moved to zeal-headless; use zeal-headless with the same arguments.\n");
        return 1;
    }
    return 0;
}
