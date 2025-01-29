#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
/* Required for file operations */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* **** */
#include "hw/flash.h"
#include "utils/paths.h"


static uint8_t flash_read(device_t* dev, uint32_t addr)
{
    flash_t* f = (flash_t*) dev;
    if (addr >= f->size) {
        printf("[FLASH] Invalid read size: %08x\n", addr);
        return 0;
    }
    return f->data[addr];
}


static void flash_write(device_t* dev, uint32_t addr, uint8_t data)
{
    // TODO: Support flashing at runtime?
    (void) dev;
    (void) addr;
    (void) data;
}


int flash_init(flash_t* f)
{
    if (f == NULL) {
        return 1;
    }
    memset(f, 0, sizeof(*f));
    f->size = NOR_FLASH_SIZE_KB;

#if CONFIG_NOR_FLASH_DYNAMIC_ARRAY
    f->data = malloc(f->size);
    if (f->data == NULL) {
        printf("[FLASH] ERROR: could not allocate enough memory for the NOR flash\n");
        return 2;
    }
#endif

    device_init_mem(DEVICE(f), "nor_flash_dev", flash_read, flash_write, f->size);
    return 0;
}


int flash_load_from_file(flash_t* flash, const char* name)
{
    if (flash == NULL /* || name == NULL */) {
        return -1;
    }

    char rom_path[PATH_MAX];
    if(name != NULL) {
        snprintf(rom_path, sizeof(rom_path), "%s", name);
    } else {
        char path_buffer[PATH_MAX];
        get_executable_dir(path_buffer, sizeof(path_buffer));
        snprintf(rom_path, sizeof(rom_path), "%s/%s", path_buffer, "roms/default.img");
    }

    int fd = open(rom_path, O_RDONLY);
    if (fd < 0) {
        perror("[FLASH] Could not open file to load");
        return fd;
    }

    int rd = read(fd, flash->data, flash->size);
    if (rd < 0) {
        perror("[FLASH] Could not read file to load");
        close(fd);
        return rd;
    }

    printf("[FLASH] %s loaded successfully\n", rom_path);

    close(fd);
    return 0;
}


int flash_save_to_file(flash_t* flash, const char* name)
{
    if (flash == NULL || name == NULL) {
        return -1;
    }

    int fd = open(name, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (fd < 0) {
        perror("[FLASH] Could not create file to dump NOR flash");
        return fd;
    }

    int wr = write(fd, flash->data, flash->size);
    if (wr < 0) {
        perror("[FLASH] Could not dump to file");
        close(fd);
        return wr;
    }

    printf("[FLASH] Dump saved to %s successfully\n", name);

    close(fd);
    return 0;
}
