/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
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
/* **** */
#include "utils/helpers.h"
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

typedef enum {
    STATE_IDLE,
    STATE_SOFTWARE_ID,
    /* State reached after *(0x5555)=0xAA operation is detected.
     * This operation is common to all erase, software id and write commands */
    STATE_SPECIAL_STEP0,
    STATE_SPECIAL_STEP1,    /* Similarly, with *(0x2AAAA)=0x55 */
    STATE_SPECIAL_STEP2,    /* Similarly, only relevant for erase command */
    STATE_SPECIAL_STEP3,    /* Similarly, only relevant for erase command */
    /* In this state, writing a byte is allowed, then a delay is applied */
    STATE_PERFORM_WRITE,
    /* During this state, the flash is being written */
    STATE_PERFORM_WRITE_DELAY,
    /* Ready to erase a block/sector */
    STATE_PERFORM_ERASE,
    /* Erasing a sector, delaying */
    STATE_PERFORM_ERASE_DELAY,
} fsm_state_t;

static uint8_t flash_read(device_t* dev, uint32_t addr)
{
    flash_t* f = (flash_t*) dev;

    switch (f->state) {
        case STATE_PERFORM_ERASE_DELAY:
            return 0xff;
        case STATE_PERFORM_WRITE_DELAY: {
            /* DQ7 has already been flipped before programming the write */
            const uint8_t ret = f->writing_byte;
            /* Bit 6 must be flipped here */
            f->writing_byte ^= 0x40;
            return ret;
        }
        case STATE_SOFTWARE_ID:
            /**
             * On address 0, return SST Manufacturer's ID: 0xBF
             * On address 1, return the Device ID: 0xB6 (SST39SF020)
             */
            if (addr == 0) return 0xBF;
            if (addr == 1) return 0xB6;
            /* TODO: Verify on real hardware the behavior here */
            return 0xFF;
        default:
            break;
    }

    if (addr >= f->size) {
        log_err_printf("[FLASH] Invalid read size: %08x\n", addr);
        return 0;
    }
    return f->data[addr];
}

static uint8_t flash_debug_read(device_t* dev, uint32_t addr)
{
    flash_t* f = (flash_t*) dev;
    if (addr >= f->size) {
        log_err_printf("[FLASH] Invalid debug read size: %08x\n", addr);
        return 0;
    }
    return f->data[addr];
}

static void flash_write(device_t* dev, uint32_t addr, uint8_t data)
{
    flash_t* f = (flash_t*) dev;

    switch (f->state) {
        case STATE_IDLE:
            if (addr == 0x5555 && data == 0xaa) {
                f->state = STATE_SPECIAL_STEP0;
            }
            break;
        case STATE_SPECIAL_STEP0:
            if (addr == 0x2aaa && data == 0x55) {
                f->state = STATE_SPECIAL_STEP1;
            }
            break;
        case STATE_SPECIAL_STEP1:
            if (addr == 0x5555 && data == 0x90) {
                f->state = STATE_SOFTWARE_ID;
            } else if (addr == 0x5555 && data == 0xa0) {
                f->state = STATE_PERFORM_WRITE;
            } else if (addr == 0x5555 && data == 0x80) {
                f->state = STATE_SPECIAL_STEP2;
            }
            break;
        case STATE_SPECIAL_STEP2:
            if (addr == 0x5555 && data == 0xaa) {
                f->state = STATE_SPECIAL_STEP3;
            }
            break;
        case STATE_SPECIAL_STEP3:
            if (addr == 0x2aaa && data == 0x55) {
                f->state = STATE_PERFORM_ERASE;
            }
            break;

        case STATE_PERFORM_WRITE:
            /* The NOR flash accepts writing a byte! Only bits that are 1 can be set to 0.
             * Write the value right now to simplify the logic after */
            log_printf("[FLASH] Writing byte 0x%x (& %x = %x) @ 0x%x\n", data, f->data[addr], data & f->data[addr], addr);
            f->data[addr] &= data;
            f->dirty = 1;
            /* The byte being written must have bit 7 flipped, DQ6 must be toggled at each read */
            f->writing_byte = data ^ 0x80;
            /* Writing a byte takes 20us on real hardware, register a callback to actually reflect this */
            f->ticks_remaining = us_to_tstates(20);
            f->state = STATE_PERFORM_WRITE_DELAY;
            break;

        case STATE_PERFORM_ERASE:
            if (data == 0x30) {
                /* Erase sector transaction! */
                f->state = STATE_PERFORM_ERASE_DELAY;
                /* Erasing a sector takes 25ms on real hardware */
                f->ticks_remaining = us_to_tstates(25000);
                /* Get the corresponding 4KB-sector to erase out of the 22-bit address */
                const uint32_t sector = addr & 0x3ff000;
                log_printf("[FLASH] Erasing sector %d @ address 0x%x\n", sector / 4096, sector);
                f->dirty = 1;
                memset(&f->data[sector], 0xff, 4096);
            } else if (data == 0x10 && addr == 0x5555) {
                /* Chip erase! */
                log_printf("[FLASH] Erasing chip\n");
                f->state = STATE_PERFORM_ERASE_DELAY;
                /* Erasing the chip takes 100ms on real hardware */
                f->ticks_remaining = us_to_tstates(100000);
                f->dirty = 1;
                memset(f->data, 0xff, f->size);
            } else {
                /* Invalid state, try again */
                f->state = STATE_IDLE;
                flash_write(dev, addr, data);
            }
            break;

        case STATE_PERFORM_ERASE_DELAY:
        case STATE_PERFORM_WRITE_DELAY:
            /* Delaying the write to simulate a real NOR flash */
            break;

        case STATE_SOFTWARE_ID:
            if (data == 0xf0) {
                f->state = STATE_IDLE;
            }
            break;

        default:
            /* The combination was invalid, reset the state and retry, else,
             * the current byte being written would be lost and not part of the FSM. */
            f->state = STATE_IDLE;
            flash_write(dev, addr, data);
            break;
    }
}


int flash_init(flash_t* f)
{
    if (f == NULL) {
        return 1;
    }
    /* Empty flash contains FF bytes*/
    memset(f, 0xFF, sizeof(*f));
    f->size = NOR_FLASH_SIZE_KB;
    f->state = STATE_IDLE;
    f->dirty = 0;

#if CONFIG_NOR_FLASH_DYNAMIC_ARRAY
    f->data = malloc(f->size);
    if (f->data == NULL) {
        log_err_printf("[FLASH] ERROR: could not allocate enough memory for the NOR flash\n");
        return 2;
    }
#endif

    device_init_mem_debug(DEVICE(f), "nor_flash_dev", flash_read, flash_write, flash_debug_read, f->size);
    return 0;
}

void flash_tick(flash_t* flash, int elapsed_tstates)
{
    if (flash->state == STATE_PERFORM_ERASE_DELAY || flash->state == STATE_PERFORM_WRITE_DELAY) {
        flash->ticks_remaining -= elapsed_tstates;
        if (flash->ticks_remaining <= 0) {
            flash->state = STATE_IDLE;
        }
    }
}

static inline uint16_t flash_dereference(flash_t* flash, uint16_t os_addr, uint16_t data_addr)
{
    uint8_t* addr = &flash->data[os_addr + data_addr];
    return (addr[0]) | (addr[1] << 8);
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

    for (size_t i = 0; i < flash->size; i += 16384) {
        /* The address 0x4 of the page shall contain the configuration structure of the OS */
        int config_addr = flash->data[i + config_offset] | flash->data[i + config_offset];
        /* The config is always with the first 4KB but after the reset vectors, make sure the
         * first byte is referring to Zeal 8-bit computer (1) */
        if (config_addr >= 0x40 && config_addr < 0x1000 && flash->data[i + config_addr] == zeal_computer_target) {
            log_printf("[FLASH] Zeal 8-bit OS found at offset 0x%lx\n", i);
            if (out_config_addr) {
                *out_config_addr = config_addr;
            }
            return (int) i;
        }
    }

    return -1;
}

/**
 * @brief Check if the given path is a correct "init" path for Zeal 8-bit OS
 */
static int flash_is_init_path(const char *s)
{
    if (s == NULL) {
        return 0;
    }

    /* Make sure it points to the romdisk */
    if ((s[0] != 'A' && s[0] != 'a') || s[1] != ':' || s[2] != '/') {
        return 0;
    }

    /* After prefix: max 16 printable chars (romdisk path) */
    for (size_t i = 0; i < 16; i++) {
        int c = s[i + 3];
        if (c == '\0') {
            return 1;
        }
        if (!isprint(c)) {
            return 0;
        }
    }

    return 1;
}


static int flash_override_romdisk(flash_t* flash, const char* userprog_filename)
{
    int err = 0;
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
        char *endptr;
        size_t address = strtoul(comma + 1, &endptr, 16);
        if (address >= flash->size || *endptr != '\0') {
            log_err_printf("[FLASH] Invalid user file address, ignoring\n");
        } else {
            romdisk_offset = address;
        }
    } else {
        /* Make the assumption romdisk is right after the OS */
        romdisk_offset = zealos_offset + 0x4000;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_perror("[FLASH] Could not open user file: %s\n", path_sanitize(path));
        err = fd;
        goto ret;
    }

    /* Get the file size and make sure it's not too big for the flash */
    struct stat st;
    if (fstat(fd, &st)) {
        log_perror("[FLASH] Could not stat user file: %s\n", path_sanitize(path));
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
    log_printf("Loading user program as %s\n", entry.name);

    memcpy(romdisk, &entry, sizeof(entry));

    /* Read the data directly in flash */
    int rd = read(fd, flash->data + romdisk_offset + romdisk_header, flash->size);
    if (rd < 0) {
        log_perror("[FLASH] Could not read user file: %s\n", path);
        err = rd;
        goto ret_close;
    }

    log_printf("[FLASH] User program %s loaded successfully @ 0x%x\n", path_sanitize(path), romdisk_offset);

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

    if (rom_filename != NULL) {
        snprintf(rom_path, sizeof(rom_path), "%s", rom_filename);
    } else {
        const char* env_rom = getenv("ZEAL_NATIVE_ROM");
        if (env_rom != NULL && env_rom[0] != '\0' && access(env_rom, F_OK) == 0) {
            // Environment variable exists and file is accessible
            snprintf(rom_path, sizeof(rom_path), "%s", env_rom);
        } else {
            // Check $HOME/.zeal8bit/roms/default.img
            const char* config_dir = get_config_dir();
            if (config_dir != NULL) {
                snprintf(rom_path, sizeof(rom_path), "%s/roms/default.img", config_dir);
                log_printf("[FLASH] Trying to load %s\n", path_sanitize(rom_path));
                if (access(rom_path, F_OK) != 0) {
                    // Fallback to relative path
                    const char* default_name = "roms/default.img";
                    log_printf("[FLASH] Trying to load (install-dir) %s\n", default_name);
                    if (get_install_dir_file(rom_path, default_name) == 0) {
                        log_err_printf("[FLASH] Could not get %s\n", default_name);
                        return -1;
                    }
                }
            } else {
                // HOME not set, fallback to relative path
                const char* default_name = "roms/default.img";
                log_printf("[FLASH] Trying to load %s\n", default_name);
                if (get_install_dir_file(rom_path, default_name) == 0) {
                    log_err_printf("[FLASH] Could not get (install-dir) %s\n", default_name);
                    return -1;
                }
            }
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

    log_printf("[FLASH] %s loaded successfully\n", path_sanitize(rom_path));

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

    /* No change performed on the flash, nothing to write */
    if (flash->dirty == 0) {
        return 0;
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
