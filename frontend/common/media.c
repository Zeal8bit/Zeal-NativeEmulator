/*
 * SPDX-FileCopyrightText: 2025-2026 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "frontend/common/media.h"
#include "utils/log.h"

frontend_media_t frontend_media = { .cf_fd = -1 };

static void eeprom_persist(void* userdata, uint32_t page_start, const uint8_t* data)
{
    FILE* file = userdata;
    if (file) {
        if (fseek(file, page_start, SEEK_SET) != 0) {
            log_perror("[EEPROM] Could not seek to completed page");
        } else {
            const size_t written = fwrite(data, 1, AT24C512_PAGE, file);
            /* Flush this completed I2C page transaction, not each guest byte.
             * WasmFS OPFS commits its underlying write before fflush returns. */
            const int flushed = fflush(file);
            if (written != AT24C512_PAGE) {
                log_err_printf("[EEPROM] Could only write %zu/%d bytes of completed page\n", written, AT24C512_PAGE);
            }
            if (flushed != 0) {
                log_perror("[EEPROM] Could not flush completed page");
            }
        }
    }
}

int frontend_eeprom_init(frontend_media_t* media, at24c512_t* eeprom, const char* filename)
{
    media->eeprom_device = eeprom;
    int err = at24c512_init(eeprom);
    if (err) return err;
    if (filename) {
        FILE* file = fopen(filename, "rb+");
        if (file) {
            size_t count = fread(eeprom->data, 1, AT24C512_SIZE, file);
            log_printf("[EEPROM] Loaded from %s\n", filename);
            if (count < AT24C512_SIZE) {
                log_err_printf("[EEPROM] Warning: image size is smaller than EEPROM size\n");
            }
            media->eeprom = file;
            eeprom->userdata = file;
            eeprom->persist = eeprom_persist;
        } else {
            log_err_printf("[EEPROM] Could not open image %s\n", filename);
        }
    }

    return 0;
}

void frontend_eeprom_close(frontend_media_t* media)
{
    if (media->eeprom_device) {
        media->eeprom_device->persist = NULL;
        media->eeprom_device->userdata = NULL;
        media->eeprom_device = NULL;
    }
    if (media->eeprom) {
        fflush(media->eeprom);
        fclose(media->eeprom);
        media->eeprom = NULL;
    }
}

static bool cf_read_sector(void* userdata, uint64_t offset, uint8_t* data)
{
    frontend_media_t* media = userdata;
    if (lseek(media->cf_fd, offset, SEEK_SET) == -1) {
        log_perror("[COMPACTFLASH] ERROR '%s' %s\n", media->cf_name, "seek()");
        return false;
    }
    int count = read(media->cf_fd, data, 512);
    if (count == -1) {
        log_perror("[COMPACTFLASH] ERROR '%s' %s\n", media->cf_name, "read()");
        return false;
    }
    if (count < 512) memset(data + count, 0, 512 - count);
    return true;
}

static bool cf_write_sector(void* userdata, uint64_t offset, const uint8_t* data)
{
    frontend_media_t* media = userdata;
    if (lseek(media->cf_fd, offset, SEEK_SET) == -1) {
        log_perror("[COMPACTFLASH] ERROR '%s' %s\n", media->cf_name, "seek()");
        return false;
    }
    if (write(media->cf_fd, data, 512) == -1) {
        log_perror("[COMPACTFLASH] ERROR '%s' %s\n", media->cf_name, "write()");
        return false;
    }
    return true;
}

int frontend_cf_init(frontend_media_t* media, compactflash_t* cf, const char* file_name)
{
    struct stat st;

    if (file_name == NULL) {
        /* No CompactFlash file specified */
        return 1;
    }

    int fd = open(file_name, O_RDWR);
    if (fd < 0) {
        log_perror("[COMPACTFLASH] Could not open file");
        /* Continue without CF emulation */
        return 1;
    }

    if (fstat(fd, &st) == -1) {
        log_perror("[COMPACTFLASH] Could not stat file");
        close(fd);
        return 1;
    }

    if (st.st_size < 1024 * 1024) {
        log_err_printf("[COMPACTFLASH] Image must be at least 1MB big\n");
        close(fd);
        return 1;
    }

    /* Round up the total amount of sectors */
    size_t total_sectors = (st.st_size + 511) / 512;

    compactflash_init(cf, total_sectors);
    media->cf_fd = fd;
    media->cf_name = strdup(file_name);
    cf->userdata = media;
    media->cf_device = cf;
    cf->read_sector = cf_read_sector;
    cf->write_sector = cf_write_sector;
    return 0;
}

static bool tf_seek(void* userdata, uint32_t sector)
{
    const uint64_t offset = (uint64_t) sector * 512;
    if (offset > LONG_MAX) {
        log_err_printf("[TF] Sector offset exceeds host file range: 0x%x\n", sector);
        return false;
    }
    if (fseek(userdata, (long) offset, SEEK_SET) != 0) {
        log_perror("[TF] Could not seek into image");
        return false;
    }
    return true;
}

static size_t tf_read(void* userdata, uint8_t* data)
{
    return fread(data, 1, 512, userdata);
}

static size_t tf_write(void* userdata, const uint8_t* data)
{
    return fwrite(data, 1, 512, userdata);
}

static int tf_flush(void* userdata)
{
    int result = fflush(userdata);
    if (result != 0) log_perror("[TF] Could not flush completed sector");
    return result;
}

int frontend_tf_load(frontend_media_t* media, zvb_spi_t* spi, const char* filename)
{
    if (spi == NULL || filename == NULL) {
        return 1;
    }

    /* Open it in both read and write */
    FILE* image = fopen(filename, "r+b");
    if (image == NULL) {
        log_perror("[TF] Could not open TF Card image");
        return 1;
    }

    /* Save the size of the file */
    if (fseek(image, 0, SEEK_END) != 0) {
        fclose(image);
        return 1;
    }
    const long size = ftell(image);
    if (size < 0 || fseek(image, 0, SEEK_SET) != 0) {
        fclose(image);
        return 1;
    }
    const zvb_tf_backend_t backend = {
        .userdata = image,
        .size     = (size_t) size,
        .seek = tf_seek,
        .read = tf_read,
        .write = tf_write,
        .flush = tf_flush,
    };
    zvb_spi_bind_tf(spi, &backend);
    if (media->tf) fclose(media->tf);
    media->tf = image;
    media->tf_device = spi;
    log_printf("[TF] %s loaded successfully\n", filename);
    return 0;
}


void frontend_tf_close(frontend_media_t* media)
{
    if (media->tf_device) {
        zvb_spi_bind_tf(media->tf_device, &(zvb_tf_backend_t){0});
        media->tf_device = NULL;
    }
    if (media->tf) {
        fclose(media->tf);
        media->tf = NULL;
    }
}

void frontend_media_close(frontend_media_t* media)
{
    frontend_eeprom_close(media);
    frontend_tf_close(media);
    if (media->cf_device) {
        media->cf_device->read_sector = NULL;
        media->cf_device->write_sector = NULL;
        media->cf_device->userdata = NULL;
        media->cf_device = NULL;
    }
    if (media->cf_fd >= 0) {
        close(media->cf_fd);
        media->cf_fd = -1;
    }
    free(media->cf_name);
    media->cf_name = NULL;
}
