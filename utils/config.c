#define RINI_VALUE_DELIMITER '='
#define RINI_IMPLEMENTATION

#include "utils/config.h"
#include "hw/zvb/zvb.h"
#include "utils/paths.h"
#include "raylib.h"

config_t config = {
    .arguments = {
        .config_path = "zeal.ini",
        .rom_filename = NULL,
        .hostfs_path = ".",
        .config_save = false,
    },

    .debugger = {
        .enabled = false,
    },

    .window = {
        .width = -1,
        .height = -1,
        .x = -1,
        .y = -1,
        .display = -1,
    },
};


void config_debug(void) {
    printf("== CONFIG ==\n");

    printf("\n");
    printf("=== command line ===\n");
    printf("  config_path: %s\n", config.arguments.config_path);
    printf(" rom_filename: %s\n", config.arguments.rom_filename);
    printf("  hostfs_path: %s\n", config.arguments.hostfs_path);
    printf("     map_file: %s\n", config.arguments.map_file);
    printf("debug_enabled: %s\n", config.debugger.enabled == DEBUGGER_STATE_ARG ? "True" : "False");
    printf("  config_save: %s\n", config.arguments.config_save ? "True" : "False");

    printf("\n");
    printf("=== debugger ===\n");
    printf("enabled: %s\n", config.debugger.enabled == DEBUGGER_STATE_CONFIG ? "True" : "False");

    printf("\n");
    printf("=== window ===\n");
    printf("  width: %d\n", config.window.width);
    printf(" height: %d\n", config.window.height);
    printf("      x: %d\n", config.window.x);
    printf("      y: %d\n", config.window.y);
    printf("display: %d\n", config.window.display);

    printf("\n\n");
}

int usage(const char* progname)
{
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("\nOptions:\n");
    printf("  -c, --config <file>    Zeal Config\n");
    printf("  -s, --save <file>      Save * arguments to Zeal Config\n");
    printf("  -r, --rom <file>       * Load ROM file\n");
    printf("  -H, --hostfs <path>    Set host filesystem path\n");
    printf("  -m, --map <file>       Load memory map file (for debugging)\n");
    printf("  -g, --debug            * Enable debug mode\n");
    printf("  -v, --verbose          Verbose console output\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s --rom game.bin --map mem.map --debug\n", progname);

    return 1;
}

int parse_command_args(int argc, char* argv[])
{
    int opt;

    struct option long_options[] = {
        { "config", required_argument, 0, 'c'},
        {    "rom", required_argument, 0, 'r'},
        { "hostfs", required_argument, 0, 'H'},
        {    "map", required_argument, 0, 'm'},
        {  "debug", required_argument, 0, 'g'},
        {   "save",       no_argument, 0, 's'},
        {"verbose",       no_argument, 0, 'v'},
        {  "help",        no_argument, 0, 'h'},
        {        0,                 0, 0,   0}
    };

    while ((opt = getopt_long(argc, argv, "c:r:H:m:sgvh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                config.arguments.config_path = optarg;
                break;
            case 's':
                config.arguments.config_save = true;
                break;
            case 'r':
                config.arguments.rom_filename = optarg;
                break;
            case 'h':
                return usage(argv[0]);
            case 'H':
                config.arguments.hostfs_path = optarg;
                break;
            case 'm':
                config.arguments.map_file = optarg;
                break;
            case 'g':
                // did user disable the debug mode (override config?)
                if(optarg == NULL) {
                    config.debugger.enabled = DEBUGGER_STATE_ARG;
                } else if(optarg[0] == '0' && optarg[1] == '\0') {
                    config.debugger.enabled = DEBUGGER_STATE_ARG_DISABLE;
                } else {
                    config.debugger.enabled = DEBUGGER_STATE_ARG;
                }
                break;
            case 'v':
                config.arguments.verbose = true;
                break;
            case '?':
                // Handle unknown options
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                return 1;
        }
    }

    return 0;
}

void config_parse_file(const char* file) {
    if(!path_exists(file)) return;

    rini_config ini = rini_load_config(file);

    if(config.arguments.rom_filename == NULL) {
        config.arguments.rom_filename = rini_get_config_value_text_fallback(ini, "ROM_FILENAME", NULL);
    }

    config.debugger.config_enabled = rini_get_config_value_fallback(ini, "DEBUG_ENABLED", DEBUGGER_STATE_DISABLED);
    if(config.debugger.enabled != DEBUGGER_STATE_ARG && config.debugger.enabled != DEBUGGER_STATE_ARG_DISABLE)
        config.debugger.enabled = config.debugger.config_enabled;


    config.window.width = rini_get_config_value_fallback(ini, "WIN_WIDTH", -1);
    config.window.height = rini_get_config_value_fallback(ini, "WIN_HEIGHT", -1);
    config.window.display = rini_get_config_value_fallback(ini, "WIN_DISPLAY", -1);
    config.window.x = rini_get_config_value_fallback(ini, "WIN_POS_X", -1);
    config.window.y = rini_get_config_value_fallback(ini, "WIN_POS_Y", -1);

    rini_unload_config(&ini);
}

int config_save() {
    rini_config ini = rini_load_config(NULL);

    // main header
    rini_set_config_comment_line(&ini, NULL);
    rini_set_config_comment_line(&ini, "Zeal Native Emulator");
    rini_set_config_comment_line(&ini, NULL);

    if(config.arguments.config_save) {
        // config.arguments header
        config_arguments_t *args = &config.arguments;
        rini_set_config_comment_line(&ini, "Arguments");
        if(args->rom_filename != NULL)
            rini_set_config_value_text(&ini, "ROM_FILENAME", args->rom_filename, "ROM Filename");

        if(config.debugger.enabled == DEBUGGER_STATE_ARG) {
            config.debugger.config_enabled = true;
        } else if(config.debugger.enabled == DEBUGGER_STATE_ARG_DISABLE) {
            config.debugger.enabled = false;
        }
    }

    if(config.debugger.enabled) {
        rini_set_config_value(&ini, "DEBUG_ENABLED", config.debugger.config_enabled, "Debug Enabled");
    }

    // config.window header
    config_window_t *window = &config.window;
    rini_set_config_comment_line(&ini, "Main Window");
    rini_set_config_value(&ini, "WIN_WIDTH", window->width, "Width");
    rini_set_config_value(&ini, "WIN_HEIGHT", window->height, "Height");
    rini_set_config_value(&ini, "WIN_POS_X", window->x, "X Position");
    rini_set_config_value(&ini, "WIN_POS_Y", window->y, "Y Position");
    rini_set_config_value(&ini, "WIN_DISPLAY", window->display, "Display Number");

    rini_save_config(ini, config.arguments.config_path);
    rini_unload_config(&ini);

    return 0; // TODO: error checking to ensure things worked?
}

void config_window_update(bool user_enabled) {
    bool config_enabled = config.debugger.enabled == DEBUGGER_STATE_CONFIG;
    bool update_rect = user_enabled == config_enabled;
    if(user_enabled && config.arguments.config_save && config.debugger.enabled == DEBUGGER_STATE_ARG) update_rect = true;

    if(update_rect) {
        config.window.width = GetScreenWidth();
        config.window.height = GetScreenHeight();
        Vector2 position = GetWindowPosition();
        config.window.x = position.x;
        config.window.y = position.y;
    }
    config.window.display = GetCurrentMonitor();
}

void config_window_set(void) {
    int d = config.window.display >= 0 ? config.window.display : GetCurrentMonitor();
    SetWindowMonitor(d);

    Vector2 aspect = { .x = 4, .y = 3 };
    Vector2 window_size = {
        .x = config.window.width,
        .y = config.window.height,
    };

    if(window_size.x < 0 && window_size.y < 0) {
        // default
        if(config_debugger_enabled()) {
            window_size.x = (ZVB_MAX_RES_WIDTH * 2);
            window_size.y = (ZVB_MAX_RES_HEIGHT * 2);
        } else {
            window_size.x = ZVB_MAX_RES_WIDTH;
            window_size.y = ZVB_MAX_RES_HEIGHT;
        }
    } else {
        // calculate aspect for missing size
        if(window_size.x < 0) window_size.x = (window_size.y * aspect.x) / aspect.y;
        if(window_size.y < 0) window_size.y = (window_size.x * aspect.y) / aspect.x;
    }
    SetWindowSize(window_size.x, window_size.y);

    Vector2 screen_offset = GetMonitorPosition(d);
    Vector2 screen = {
        .x = GetMonitorWidth(d),
        .y = GetMonitorHeight(d),
    };

    int x = config.window.x;
    int y = config.window.y;

    // calculate center if either coordinate is not set
    if(x < 0) x = screen_offset.x + ((screen.x - window_size.x) / 2);
    if(y < 0) y = screen_offset.y + ((screen.y - window_size.y) / 2);
    SetWindowPosition(x, y);
}
