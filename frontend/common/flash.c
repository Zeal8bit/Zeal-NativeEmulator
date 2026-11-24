/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
/* Required for file operations */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
/* **** */
#include "utils/helpers.h"
#include "hw/flash.h"
#include "utils/paths.h"
#include "utils/log.h"

#include "frontend/common/media.h"

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

static inline uint16_t flash_dereference(flash_t* flash, uint16_t os_addr, uint16_t data_addr)
{
    uint8_t* address = &flash->data[os_addr + data_addr];
    return (address[0]) | (address[1] << 8);
}

/**
 * @brief Find the address of the page where Zeal 8-bit OS is flashed
 *
 * @param flash Pointer to the flash isntance
 * @param out_config_addr Populated with the relative (to the OS) address of the config structure
 */
static int flash_find_os_page(flash_t* flash, uint32_t* out_config_addr)
{
    const int config_offset = 4;
    const int zeal_computer_target = 1;

    for (size_t page_offset = 0; page_offset < flash->size; page_offset += 16384) {
        /* The address 0x4 of the page shall contain the configuration structure of the OS */
        int config_addr = flash->data[page_offset + config_offset] | flash->data[page_offset + config_offset];
        /* The config is always with the first 4KB but after the reset vectors, make sure the
         * first byte is referring to Zeal 8-bit computer (1) */
        if (config_addr >= 0x40 && config_addr < 0x1000 && flash->data[page_offset + config_addr] == zeal_computer_target) {
            log_printf("[FLASH] Zeal 8-bit OS found at offset 0x%x\n", (uint32_t) page_offset);
            if (out_config_addr) {
                *out_config_addr = config_addr;
            }
            return (int) page_offset;
        }
    }

    return -1;
}

/**
 * @brief Check if the given path is a correct "init" path for Zeal 8-bit OS
 */
static int flash_is_init_path(const char* init_path)
{
    if (init_path == NULL) {
        return 0;
    }

    /* Make sure it points to the romdisk */
    if ((init_path[0] != 'A' && init_path[0] != 'a') || init_path[1] != ':' || init_path[2] != '/') {
        return 0;
    }

    /* After prefix: max 16 printable chars (romdisk path) */
    for (size_t character_index = 0; character_index < 16; character_index++) {
        int character = init_path[character_index + 3];
        if (character == '\0') {
            return 1;
        }
        if (!isprint(character)) {
            return 0;
        }
    }

    return 1;
}


static int flash_override_romdisk(flash_t* flash, const char* userprog_filename)
{
    int error = 0;
    int romdisk_offset = 0;
    const int romdisk_header = 64;
    uint32_t config_addr = 0;
    char* path = strdup(userprog_filename);
    if (path == NULL) {
        log_err_printf("[FLASH] No more memory!\n");
        return -1;
    }

    /* Look for Zela 8-bit OS offset in the flash */
    const int zealos_offset = flash_find_os_page(flash, &config_addr);
    if (zealos_offset == -1) {
        log_err_printf("[FLASH] Could not find Zeal 8-bit OS in the given ROM. Cannot override init program.\n");
        return 1;
    }

    /* Check if the parameter provides a romdisk address */
    char* comma = strchr(path, ',');
    if (comma) {
        /* Make the filename end before the address */
        *comma = 0;
        char* address_end;
        size_t address = strtoul(comma + 1, &address_end, 16);
        if (address >= flash->size || *address_end != '\0') {
            log_err_printf("[FLASH] Invalid user file address, ignoring\n");
        } else {
            romdisk_offset = address;
        }
    } else {
        /* Make the assumption romdisk is right after the OS */
        romdisk_offset = zealos_offset + 0x4000;
    }

    int file_descriptor = open(path, O_RDONLY);
    if (file_descriptor < 0) {
        log_perror("[FLASH] Could not open user file: %s\n", path_sanitize(path));
        error = file_descriptor;
        goto ret;
    }

    /* Get the file size and make sure it's not too big for the flash */
    struct stat file_status;
    if (fstat(file_descriptor, &file_status)) {
        log_perror("[FLASH] Could not stat user file: %s\n", path_sanitize(path));
        error = -1;
        goto ret_close;
    }
    const size_t size = file_status.st_size;

    if (size > (flash->size - romdisk_offset - romdisk_header)) {
        log_err_printf("[FLASH] User file is too big to fit in ROM\n");
        error = -1;
        goto ret_close;
    }

    /* Generate a small header for the romdisk */
    uint8_t* romdisk = flash->data + romdisk_offset;
    /* FIXME: Made the assumption that the host CPU is little-endian */
    romdisk_entry_t entry = {
        /* Single entry in the romdisk */
        .entry = 1,
        .size = size,
        .offset = romdisk_header,
        /* Ignore the date, let them be 0 */
    };
    /* Find the name pointer of the init program from the configuration structure */
    const uint16_t os_init_addr = flash_dereference(flash, zealos_offset, config_addr + 0xa);
    const char* os_init_path = (const char*) (&flash->data[zealos_offset + os_init_addr]);
    if (flash_is_init_path(os_init_path)) {
        /* Escape the A:/ prefix */
        snprintf(entry.name, sizeof(entry.name), "%s", os_init_path + 3);
    } else {
        strcpy(entry.name, "init.bin");
    }
    v_log_printf(1, "[FLASH] Loading user program as %s\n", entry.name);

    memcpy(romdisk, &entry, sizeof(entry));

    /* Read the data directly in flash */
    int bytes_read = read(file_descriptor, flash->data + romdisk_offset + romdisk_header, flash->size);
    if (bytes_read < 0) {
        log_perror("[FLASH] Could not read user file: %s\n", path);
        error = bytes_read;
        goto ret_close;
    }

    log_printf("[FLASH] User program %s loaded successfully @ 0x%x\n", path_sanitize(path), romdisk_offset);

    error = 0;
    /* Fall-through */
ret_close:
    close(file_descriptor);
ret:
    free(path);
    return error;
}


int flash_load_from_file(flash_t* flash, const char* rom_filename, const char* userprog_filename)
{
    if (flash == NULL) {
        return -1;
    }
    if (rom_filename == NULL) {
        log_err_printf("[FLASH] No ROM image found\n");
        return -1;
    }

    int file_descriptor = open(rom_filename, O_RDONLY);
    if (file_descriptor < 0) {
        log_perror("[FLASH] Could not open file to load");
        return file_descriptor;
    }

    int bytes_read = read(file_descriptor, flash->data, flash->size);
    if (bytes_read < 0) {
        log_perror("[FLASH] Could not read file to load");
        close(file_descriptor);
        return bytes_read;
    }

    log_printf("[FLASH] %s loaded successfully\n", path_sanitize(rom_filename));

    close(file_descriptor);

    /* Try to load the user program, if any */
    if (userprog_filename != NULL) {
        int error = flash_override_romdisk(flash, userprog_filename);
        if (error != 0) {
            return error;
        }
    }

    flash_loaded(flash);

    return 0;
}


int flash_save_to_file(flash_t* flash, const char* name)
{
    if (flash == NULL || name == NULL) {
        return -1;
    }

    /* No change performed on the flash, nothing to write */
    if (flash->dirty == 0) {
        return 0;
    }

    const int file_descriptor = open(name, O_WRONLY | O_TRUNC | O_CREAT, 0666);
    if (file_descriptor < 0) {
        log_perror("[FLASH] Could not create file to dump NOR flash");
        return file_descriptor;
    }

    const ssize_t bytes_written = write(file_descriptor, flash->data, flash->size);
    if (bytes_written < 0 || (size_t) bytes_written != flash->size) {
        const int error = bytes_written < 0 ? errno : EIO;
        log_perror("[FLASH] Could not dump to file");
        close(file_descriptor);
        errno = error;
        return -1;
    }

    if (close(file_descriptor) < 0) {
        return -1;
    }
    flash->dirty = 0;
    log_printf("[FLASH] Dump saved to %s successfully\n", name);
    return 0;
}
