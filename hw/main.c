#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hw/zeal.h"

static zeal_t machine;

int main(int argc, char* argv[])
{
    int opt;
    const char* rom_filename = NULL;
    const char* hostfs_path = NULL;
    int code = 0;

    while ((opt = getopt(argc, argv, "r:h:")) != -1) {
        switch (opt) {
            case 'r':
                rom_filename = optarg; // Store the filename
                break;
            case 'h':
                hostfs_path = optarg; // Store the filename
                break;
            case '?':
                // Handle unknown options
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                return 1;
        }
    }

    if (rom_filename == NULL) {
        printf("No ROM file specified. Use -r <filename>.\n");
        return 1;
    }

    if (hostfs_path == NULL) {
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

    if (flash_load_from_file(&machine.rom, rom_filename)) {
        return 1;
    }

    if (hostfs_load_path(&machine.hostfs, hostfs_path)) {
        return 1;
    }

    code = zeal_run(&machine);

    return code;
}
