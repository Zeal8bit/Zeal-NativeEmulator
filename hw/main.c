#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "hw/zeal.h"

static zeal_t machine;


int usage(const char *progname) {
    printf("Usage: %s [OPTIONS]\n", progname);
    printf("\nOptions:\n");
    printf("  -r, --rom <file>       Load ROM file\n");
    printf("  -H, --hostfs <path>    Set host filesystem path\n");
    printf("  -m, --map <file>       Load memory map file (for debugging)\n");
    printf("  -g, --debug            Enable debug mode\n");
    printf("  -h, --help             Show this help message\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s --rom game.bin --map mem.map --debug\n", progname);

    return 1;
}


int main(int argc, char* argv[])
{
    zeal_opt_t zeal_opt = {
        .dbg = false
    };
    int opt;
    const char* rom_filename = NULL;
    const char* hostfs_path = NULL;
    int code = 0;

    struct option long_options[] = {
        {"rom",    required_argument, 0, 'r'},
        {"hostfs", required_argument, 0, 'H'},
        {"map",    required_argument, 0, 'm'},
        {"debug",  no_argument,       0, 'g'},
        {"help",   no_argument,       0, 'h'},
        {0,        0,                 0, 0  }
    };

    while ((opt = getopt_long(argc, argv, "r:H:m:gh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r':
                rom_filename = optarg; // Store the filename
                break;
            case 'h':
                return usage(argv[0]);
            case 'H':
                hostfs_path = optarg;
                break;
            case 'm':
                zeal_opt.map_file = optarg;
                break;
            case 'g':
                zeal_opt.dbg = true;
                break;
            case '?':
                // Handle unknown options
                fprintf(stderr, "Unknown option: -%c\n", optopt);
                return 1;
        }
    }

    if (rom_filename == NULL) {
        printf("No ROM file specified.\n");
    }

    if (hostfs_path == NULL) {
        printf("No HostFS path specified.\n");
    }

    // Process non-option arguments, if needed
    for (int i = optind; i < argc; i++) {
        printf("Non-option argument: %s\n", argv[i]);
    }

    if (zeal_init(&machine, &zeal_opt)) {
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
