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
#include "utils/log.h"


/**
 * @brief Romdisk header and first entry
 */
typedef struct {
    uint16_t entry;
    char     name[16];
    uint32_t size;
    uint32_t offset;
    uint8_t  year[2];
    uint8_t  month;
    uint8_t  day;
    uint8_t  date;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
} __attribute__((packed)) romdisk_entry_t;


static uint8_t flash_read(device_t* dev, uint32_t addr)
{
    flash_t* f = (flash_t*) dev;
    if (addr >= f->size) {
        log_err_printf("[FLASH] Invalid read size: %08x\n", addr);
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
    /* Empty flash contains FF bytes*/
    memset(f, 0xFF, sizeof(*f));
    f->size = NOR_FLASH_SIZE_KB;

#if CONFIG_NOR_FLASH_DYNAMIC_ARRAY
    f->data = malloc(f->size);
    if (f->data == NULL) {
        log_err_printf("[FLASH] ERROR: could not allocate enough memory for the NOR flash\n");
        return 2;
    }
#endif

    device_init_mem(DEVICE(f), "nor_flash_dev", flash_read, flash_write, f->size);
    return 0;
}

static int flash_override_romdisk(flash_t* flash, const char* userprog_filename)
{
    int err = 0;
    int romdisk_offset = 0x8000;
    const int romdisk_header = 64;
    char* path = strdup(userprog_filename);
    if (path == NULL) {
        log_err_printf("[FLASH] No more memory!\n");
        return -1;
    }

    /* Check if the parameter provides a romdisk address */
    char* comma = strchr(path, ',');
    if (comma) {
        /* Make the filename end before the address */
        *comma = 0;
        char *endptr;
        size_t address = strtoul(comma + 1, &endptr, 16);
        if (address >= flash->size || *endptr != '\0') {
            log_err_printf("[FLASH] Invalid user file address, ignoring\n");
        } else {
            romdisk_offset = address;
        }
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_perror("[FLASH] Could not open user file: %s\n", path);
        err = fd;
        goto ret;
    }

    /* Get the file size and make sure it's not too big for the flash */
    struct stat st;
    if (fstat(fd, &st)) {
        log_perror("[FLASH] Could not stat user file: %s\n", path);
        err = -1;
        goto ret_close;
    }
    const size_t size = st.st_size;

    if (size > (flash->size - romdisk_offset - romdisk_header)) {
        log_err_printf("[FLASH] User file is too big to fit in ROM\n");
        err = -1;
        goto ret_close;
    }

    /* Generate a small header for the romdisk */
    uint8_t* romdisk = flash->data + romdisk_offset;
    /* FIXME: Made the assumption that the host CPU is little-endian */
    const romdisk_entry_t entry = {
        /* Single entry in the romdisk */
        .entry = 1,
        .name = "init.bin",
        .size = size,
        .offset = romdisk_header,
        /* Ignore the date, let them be 0 */
    };
    memcpy(romdisk, &entry, sizeof(entry));

    /* Read the data directly in flash */
    int rd = read(fd, flash->data + romdisk_offset + romdisk_header, flash->size);
    if (rd < 0) {
        log_perror("[FLASH] Could not read user file: %s\n", path);
        err = rd;
        goto ret_close;
    }

    log_printf("[FLASH] User program %s loaded successfully @ 0x%x\n", path, romdisk_offset);

    err = 0;
    /* Fall-through */
ret_close:
    close(fd);
ret:
    free(path);
    return err;
}


int flash_load_from_file(flash_t* flash, const char* rom_filename, const char* userprog_filename)
{
    char rom_path[PATH_MAX];

    if (flash == NULL) {
        return -1;
    }

    if(rom_filename != NULL) {
        snprintf(rom_path, sizeof(rom_path), "%s", rom_filename);
    } else {
        const char* default_name = "roms/default.img";
        log_printf("[FLASH] Trying to load %s\n", default_name);
        if (get_install_dir_file(rom_path, default_name) == 0) {
            log_err_printf("[FLASH] Could not get %s\n", default_name);
            return -1;
        }
    }

    int fd = open(rom_path, O_RDONLY);
    if (fd < 0) {
        log_perror("[FLASH] Could not open file to load");
        return fd;
    }

    int rd = read(fd, flash->data, flash->size);
    if (rd < 0) {
        log_perror("[FLASH] Could not read file to load");
        close(fd);
        return rd;
    }

    log_printf("[FLASH] %s loaded successfully\n", rom_path);

    close(fd);

    /* Try to load the user program, if any */
    if (userprog_filename != NULL ) {
        return flash_override_romdisk(flash, userprog_filename);
    }

    return 0;
}


int flash_save_to_file(flash_t* flash, const char* name)
{
    if (flash == NULL || name == NULL) {
        return -1;
    }

    int fd = open(name, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (fd < 0) {
        log_perror("[FLASH] Could not create file to dump NOR flash");
        return fd;
    }

    int wr = write(fd, flash->data, flash->size);
    if (wr < 0) {
        log_perror("[FLASH] Could not dump to file");
        close(fd);
        return wr;
    }

    log_printf("[FLASH] Dump saved to %s successfully\n", name);

    close(fd);
    return 0;
}
