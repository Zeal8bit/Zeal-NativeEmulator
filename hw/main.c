#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "hw/zeal.h"
#include "utils/config.h"

static zeal_t machine;

int main(int argc, char* argv[])
{
    int code = 0;
    code = parse_command_args(argc, argv);
    if(code != 0) return code;

    config_parse_file(config.arguments.config_path);
    if(config.arguments.verbose) config_debug();

    if (config.arguments.rom_filename == NULL) {
        printf("No ROM file specified.\n");
    }

    if (config.arguments.hostfs_path == NULL) {
        printf("No HostFS path specified.\n");
    }

    // Process non-option arguments, if needed
    for (int i = optind; i < argc; i++) {
        printf("Non-option argument: %s\n", argv[i]);
    }

    if (zeal_init(&machine)) {
        printf("Error initializing the machine\n");
        return 1;
    }

    if (flash_load_from_file(&machine.rom, config.arguments.rom_filename)) {
        return 1;
    }

    if (hostfs_load_path(&machine.hostfs, config.arguments.hostfs_path)) {
        return 1;
    }

    if (config.arguments.tf_filename != NULL &&
        zvb_spi_load_tf_image(&machine.zvb.spi, config.arguments.tf_filename)) {
        return 1;
    }

    code = zeal_run(&machine);

    int saved = config_save();
    config_unload();
    if(!saved && code == 0) return saved; // ???

    return code;
}
