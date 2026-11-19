/* SPDX-License-Identifier: Apache-2.0 */
#include "frontend/common/cli.h"
#include "frontend/common/desktop.h"
#include "frontend/sdl-fltk/config.h"
#include "frontend/sdl-fltk/paths.h"
#include "utils/log.h"
#include "utils/config.h"

int main(int argc, char* argv[])
{
    const char* user_path = get_config_path();
    if (user_path) fltk_settings_args.user_path = user_path;

    const frontend_cli_option_t options[] = {
        {'c', "config", "  -c, --config <file>                User settings (default ~/.zeal8bit/" CONFIG_USER_FILENAME ")",
         .argument = &fltk_settings_args.user_path},
        {'P', "project", "  -P, --project <file>               Project settings (default ./" CONFIG_PROJECT_FILENAME ")",
         .argument = &fltk_settings_args.project_path},
        {'s', "save", "  -s, --save                         Save image paths to project settings",
         .flag = &fltk_settings_args.save_arguments},
    };
    int code = frontend_configure_graphical(argc, argv, options, sizeof(options) / sizeof(options[0]));
    if (code) return code;
    if (config_parse_file(fltk_settings_args.user_path) != 0) return 1;

    v_log_printf(2, "[CONFIG]   config_path: %s\n", fltk_settings_args.user_path);
    v_log_printf(2, "[CONFIG]  project_path: %s\n", fltk_settings_args.project_path);
    v_log_printf(2, "[CONFIG]   config_save: %s\n", fltk_settings_args.save_arguments ? "True" : "False");
    code = desktop_run(true);
    config_unload();
    return code;
}
