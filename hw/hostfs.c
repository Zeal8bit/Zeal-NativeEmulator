/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#ifndef _DEFAULT_SOURCE
    #define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>
#include <libgen.h>

#include "hw/hostfs.h"
#include "utils/log.h"
#include "utils/paths.h"

#define MIN(a,b)    ((a) < (b) ? (a) : (b))

/**
 * @brief Macros related to Zeal 8-bit OS public API
 */
#define ZOS_SUCCESS              0
#define ZOS_FAILURE              1
#define ZOS_NO_SUCH_ENTRY        4
#define ZOS_CANNOT_REGISTER_MORE 20
#define ZOS_NO_MORE_ENTRIES      21
#define ZOS_PENDING              0xFF


/**
 * @brief Operations for the Host FS
 */
#define OP_WHOAMI   0
#define OP_OPEN     1
#define OP_STAT     2
#define OP_READ     3
#define OP_WRITE    4
#define OP_CLOSE    5
#define OP_OPENDIR  6
#define OP_READDIR  7
#define OP_MKDIR    8
#define OP_RM       9
#define OP_LAST     OP_RM

#define OPERATION_REG   0xF

#define ZOS_FD_OFFSET_T     (8)
#define ZOS_FD_USER_T       (12)
#define ZOS_FL_RDONLY       (0)
#define ZOS_FL_WRONLY       (1)
#define ZOS_FL_RDWR         (2)
#define ZOS_FL_TRUNC        (1 << 2)
#define ZOS_FL_APPEND       (2 << 2)
#define ZOS_FL_CREAT        (4 << 2)


static int descriptor_valid(hostfs_fd_t* desc)
{
    return desc->raw != NULL;
}

static void set_status(zeal_hostfs_t *host, uint8_t status)
{
    host->registers[0xF] = status;
}

static void fs_whoami(zeal_hostfs_t *host)
{
    set_status(host, 0xD3);
}

/**
 * @brief Helper to always make a file name 16 bytes big
*/
static void zos_format_name(const char *input_name, char *output_name)
{
    size_t input_length = strlen(input_name);

    if (input_length < ZOS_MAX_NAME_LENGTH) {
        strncpy(output_name, input_name, ZOS_MAX_NAME_LENGTH);
        memset(output_name + input_length, '\0', ZOS_MAX_NAME_LENGTH - input_length);
    } else if (input_length > ZOS_MAX_NAME_LENGTH) {
        strncpy(output_name, input_name, ZOS_MAX_NAME_LENGTH - 1);
        output_name[ZOS_MAX_NAME_LENGTH - 1] = '~';
    } else {
        strncpy(output_name, input_name, ZOS_MAX_NAME_LENGTH);
    }
}


static char *resolve_path(const char *path, int prefix_len)
{
    if (path == NULL || strlen(path) == 0) return NULL;

    // Allocate a buffer for the resolved path
    char *resolved = calloc(1, PATH_MAX);
    if (resolved == NULL) {
        return NULL;
    }

    char *copy = strdup(path);
    if (!copy) {
        free(resolved);
        return NULL;
    }

    /* This is used for Windows, where the drive letter may be present */
    memcpy(resolved, path, prefix_len);

    char *token = strtok(copy + prefix_len + 1, "/");
    while (token != NULL) {
        if (strcmp(token, "..") == 0) {
            size_t len = strlen(resolved);
            if (len > 1) {
                /* Remove trailing slash */
                resolved[len - 1] = '\0';
            }
            char *last_slash = strrchr(resolved, '/');
            if (last_slash != NULL) *last_slash = '\0';
        } else if (strcmp(token, ".") != 0) {
            strcat(resolved, "/");
            strcat(resolved, token);
        }

        token = strtok(NULL, "/");
    }

    free(copy);
    return resolved;
}



static char *get_path(zeal_hostfs_t *host)
{
    uint16_t virt_addr = (host->registers[2] << 8) | host->registers[1];
    static char full_path[PATH_MAX];
    char path[256];
    size_t i = 0;
    while (i < sizeof(path) - 1) {
        const uint8_t byte = memory_read_byte(host->host_ops, virt_addr++);
        if (byte == 0) {
            break;
        }
        path[i++] = (char) byte;
    }
    path[i] = '\0';

    /* Concatenate the root path and the relative path */
    if ((size_t) snprintf(full_path, sizeof(full_path), "%s/%s", host->root_path, path) >= sizeof(full_path)) {
        log_err_printf("[HostFS] Path is too long, ignoring!\n");
        set_status(host, ZOS_NO_SUCH_ENTRY);
        return NULL;
    }

    /* Normalize and check the full path to avoid pointing out of the mounted directory */
    int root_length = strlen(host->root_path);
    if (host->root_path[root_length - 1] == '/') {
        root_length--;
    }

    int prefix_length = 0;
#ifdef _WIN32
    /* For Windows, let's skip the drive letter, if any, the root path does NOT have backslashes */
    if (root_length > 2 && full_path[1] == ':' && full_path[2] == '/') {
        prefix_length = 2;
    }
#endif

    char* resolved = resolve_path(full_path, prefix_length);
    if (resolved == NULL ||
        strncmp(resolved, host->root_path, root_length) != 0)
    {
        log_err_printf("[HostFS] Invalid path %s (original: %s)\n", resolved, full_path);
        free(resolved);
        set_status(host, ZOS_NO_SUCH_ENTRY);
        return NULL;
    }

    /* Copy the resolved path to the static buffer for return */
    strcpy(full_path, resolved);
    free(resolved);
    return full_path;
}

static FILE *fopen_with_flags(const char *path, int zos_flags)
{
    int open_flags = 0;

    // Determine access mode
    switch (zos_flags & 0x3) {  // Mask only the low 2 bits
        case ZOS_FL_RDONLY:
            open_flags |= O_RDONLY;
            break;
        case ZOS_FL_WRONLY:
            open_flags |= O_WRONLY;
            break;
        case ZOS_FL_RDWR:
            open_flags |= O_RDWR;
            break;
        default:
            return NULL;
    }

    // Add extra behavior flags
    if (zos_flags & ZOS_FL_CREAT)
        open_flags |= O_CREAT;
    if (zos_flags & ZOS_FL_TRUNC)
        open_flags |= O_TRUNC;
    if (zos_flags & ZOS_FL_APPEND)
        open_flags |= O_APPEND;

    // Open file with appropriate mode
    int fd = open(path, open_flags | OPEN_BINARY, 0644);
    if (fd == -1)
        return NULL;

    // Map zos_flags to fdopen mode string
    const char *mode_str = NULL;
    int is_read = (zos_flags & 0x3) == ZOS_FL_RDONLY || (zos_flags & 0x3) == ZOS_FL_RDWR;
    int is_write = (zos_flags & 0x3) == ZOS_FL_WRONLY || (zos_flags & 0x3) == ZOS_FL_RDWR;

    if (is_read && is_write) {
        mode_str = (zos_flags & ZOS_FL_APPEND) ? "a" FOPEN_BINARY "+" : "r" FOPEN_BINARY "+";
    } else if (is_write) {
        mode_str = (zos_flags & ZOS_FL_APPEND) ? "a" FOPEN_BINARY : "w" FOPEN_BINARY;
    } else if (is_read) {
        mode_str = "r" FOPEN_BINARY;
    }

    FILE *file = fdopen(fd, mode_str);
    if (!file) {
        close(fd);
        return NULL;
    }

    return file;
}

static void populate_opendir(zeal_hostfs_t *host, char* path)
{
    DIR *dir = opendir(path);

    if (!dir) {
        set_status(host, ZOS_NO_SUCH_ENTRY);
        return;
    }

    for (int i = 0; i < MAX_OPENED_FILES; i++) {
        hostfs_fd_t* fd = &host->descriptors[i];
        if (!descriptor_valid(fd)) {
            strncpy(fd->name, basename(path), ZOS_MAX_NAME_LENGTH);
#ifdef _WIN32
            strncpy(fd->path, path, sizeof(fd->path) - 1);
#endif
            fd->dir = dir;
            fd->is_dir = 1;
            host->registers[4] = (uint8_t) i;
            /* Tell the Z80 this is a directory */
            host->registers[5] = 1;
            set_status(host, ZOS_SUCCESS);
            return;
        }
    }

    closedir(dir);
    set_status(host, ZOS_CANNOT_REGISTER_MORE);
}

static void fs_open(zeal_hostfs_t *host)
{
    char *path = get_path(host);
    struct stat st;

    if (path == NULL) {
        return;
    }

    /* Check if the path points to a directory, if yes, switch to opendir */
    if (stat(path, &st) != 0) {
        set_status(host, ZOS_NO_SUCH_ENTRY);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        populate_opendir(host, path);
        return;
    }

    uint8_t flags = host->registers[0];
    FILE *file = fopen_with_flags(path, flags);
    if (!file) {
        set_status(host, ZOS_NO_SUCH_ENTRY);
        return;
    }
    for (int i = 0; i < MAX_OPENED_FILES; i++) {
        hostfs_fd_t* fd = &host->descriptors[i];
        if (!descriptor_valid(fd)) {
            strncpy(fd->name, basename(path), ZOS_MAX_NAME_LENGTH);
            fd->file = file;
            fd->is_dir = 0;
            host->registers[0] = (st.st_size >> 0) & 0xFF;
            host->registers[1] = (st.st_size >> 8) & 0xFF;
            host->registers[2] = (st.st_size >> 16) & 0xFF;
            host->registers[3] = (st.st_size >> 24) & 0xFF;
            host->registers[4] = (uint8_t) i;
            /* Tell the Z80 this is a file (not directory) */
            host->registers[5] = 0;
            set_status(host, ZOS_SUCCESS);
            return;
        }
    }
    fclose(file);
    set_status(host, ZOS_CANNOT_REGISTER_MORE);
}


static void fs_close(zeal_hostfs_t *host)
{
    const int desc = host->registers[0];
    if (desc >= MAX_OPENED_FILES) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    hostfs_fd_t* fd = &host->descriptors[desc];

    if (!fd->is_dir) {
        fclose(fd->file);
    } else {
        closedir(fd->dir);
    }

    fd->raw = NULL;
    set_status(host, ZOS_SUCCESS);
}


static void fs_stat(zeal_hostfs_t *host)
{
    struct stat st;

    const int desc = host->registers[2];
    uint16_t struct_addr = (host->registers[1] << 8) | host->registers[0];
    hostfs_fd_t* descriptor = &host->descriptors[desc];

    /* Target stat structure. For files, the pointer received points to `s_date`  */
    struct {
        uint32_t   s_size;  // in bytes
        uint8_t    s_date[8];
        char       s_name[ZOS_MAX_NAME_LENGTH];
    } zos_stat = { 0 };

    if (!descriptor_valid(descriptor)) {
        set_status(host, ZOS_NO_SUCH_ENTRY);
        return;
    }

#ifdef _WIN32
    /* On Windows, we cannot get a file descriptor out of a DIR* structure  */
    if (descriptor->is_dir) {
        if (stat(descriptor->path, &st)) {
            log_err_printf("[HostFS] Could not stat file\n");
            set_status(host, ZOS_FAILURE);
            return;
        }
    } else {
        if (fstat(fileno(descriptor->file), &st)) {
            log_err_printf("[HostFS] Could not stat file\n");
            set_status(host, ZOS_FAILURE);
            return;
        }
    }
#else
    int fd = -1;

    if (descriptor->is_dir) {
        fd = dirfd(descriptor->dir);
    } else {
        fd = fileno(descriptor->file);
    }

    if (fstat(fd, &st)) {
        log_err_printf("[HostFS] Could not stat file\n");
        set_status(host, ZOS_FAILURE);
        return;
    }
#endif

    zos_stat.s_size = st.st_size > 0x100000000L ? 0xffffffff : st.st_size;

    struct tm *tm_info = localtime(&st.st_mtime);
    zos_stat.s_date[0] = (tm_info->tm_year + 1900) / 100; // High year
    zos_stat.s_date[1] = (tm_info->tm_year + 1900) % 100; // Year
    zos_stat.s_date[2] = tm_info->tm_mon + 1;             // Month
    zos_stat.s_date[3] = tm_info->tm_mday;                // Date
    zos_stat.s_date[4] = tm_info->tm_wday;                // Day
    zos_stat.s_date[5] = tm_info->tm_hour;                // Hours
    zos_stat.s_date[6] = tm_info->tm_min;                 // Minutes
    zos_stat.s_date[7] = tm_info->tm_sec;                 // Seconds
    /* Convert to BCD (hexadecimal representation) */
    for (int i = 0; i < 8; i++) {
        zos_stat.s_date[i] = (zos_stat.s_date[i] / 10) << 4 | (zos_stat.s_date[i] % 10);
    }
    /* Write the name in the structure */
    memcpy(zos_stat.s_name, descriptor->name, ZOS_MAX_NAME_LENGTH);

    /* Write the structure to memory so that the Z80 sees it */
    if (descriptor->is_dir) {
        memory_write_bytes(host->host_ops, struct_addr, (uint8_t*) &zos_stat, sizeof(zos_stat));
    } else {
        memory_write_bytes(host->host_ops, struct_addr, (uint8_t*) &zos_stat.s_date,
                           sizeof(zos_stat) - sizeof(zos_stat.s_size));
    }

    /* Set success status */
    set_status(host, ZOS_SUCCESS);
}


static int seek_file(zeal_hostfs_t *host, uint16_t struct_addr, FILE* file)
{
    /* Get the 32-bit offset to start reading from */
    uint8_t offset[4] = { 0 };
    memory_read_bytes(host->host_ops, struct_addr + ZOS_FD_OFFSET_T, offset, sizeof(offset));
    const long seek_to = offset[3] << 24 |
                         offset[2] << 16 |
                         offset[1] << 8  |
                         offset[0];
    return fseek(file, seek_to, SEEK_SET);
}


static void fs_read(zeal_hostfs_t *host)
{
    uint8_t buffer[1024];

    const uint16_t struct_addr = (host->registers[1] << 8) | host->registers[0];
    const uint16_t buffer_addr = (host->registers[3] << 8) | host->registers[2];
    const uint16_t buffer_len  = (host->registers[5] << 8) | host->registers[4];
    size_t bytes_remaining = buffer_len;
    size_t total_bytes_written = 0;

    /* Get the abstract context from the structure */
    const int desc = memory_read_byte(host->host_ops, struct_addr + ZOS_FD_USER_T);
    if (desc >= MAX_OPENED_FILES) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    FILE *file = host->descriptors[desc].file;
    if (file == NULL) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    seek_file(host, struct_addr, file);

    while (bytes_remaining > 0) {
        size_t bytes_to_read = MIN(bytes_remaining, sizeof(buffer));
        size_t bytes_read = fread(buffer, 1, bytes_to_read, file);

        if (bytes_read == 0) {
            break;
        }

        memory_write_bytes(host->host_ops, buffer_addr + total_bytes_written, buffer, bytes_read);

        total_bytes_written += bytes_read;
        bytes_remaining -= bytes_read;
    }

    host->registers[4] = total_bytes_written & 0xFF;
    host->registers[5] = (total_bytes_written >> 8) & 0xFF;
    set_status(host, ZOS_SUCCESS);
}


static void fs_write(zeal_hostfs_t *host) {
    uint8_t buffer[1024];

    const uint16_t struct_addr = (host->registers[1] << 8) | host->registers[0];
    const uint16_t buffer_addr = (host->registers[3] << 8) | host->registers[2];
    const uint16_t buffer_len  = (host->registers[5] << 8) | host->registers[4];
    size_t bytes_remaining = buffer_len;
    size_t total_bytes_written = 0;

    /* Get the abstract context from the structure */
    const int desc = memory_read_byte(host->host_ops, struct_addr + ZOS_FD_USER_T);
    if (desc >= MAX_OPENED_FILES) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    FILE *file = host->descriptors[desc].file;
    if (file == NULL) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    seek_file(host, struct_addr, file);

    while (bytes_remaining > 0) {
        size_t bytes_to_write = MIN(bytes_remaining, sizeof(buffer));
        memory_read_bytes(host->host_ops, buffer_addr + total_bytes_written, buffer, bytes_to_write);
        fwrite(buffer, 1, bytes_to_write, file);

        total_bytes_written += bytes_to_write;
        bytes_remaining -= bytes_to_write;
    }

    host->registers[4] = total_bytes_written & 0xFF;
    host->registers[5] = (total_bytes_written >> 8) & 0xFF;
    set_status(host, ZOS_SUCCESS);
}


static void fs_mkdir(zeal_hostfs_t *host)
{
    char *path = get_path(host);
    if (path == NULL) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    if (os_mkdir(path, 0755) != 0) {
        log_perror("[HostFS] Could not create directory");
        set_status(host, ZOS_FAILURE);
        return;
    }

    set_status(host, ZOS_SUCCESS);
}


static void fs_rm(zeal_hostfs_t *host)
{
    char *path = get_path(host);
    if (path == NULL) {
        return;
    }

    if (remove(path) != 0) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    set_status(host, ZOS_SUCCESS);
}


static void fs_opendir(zeal_hostfs_t *host)
{
    char *path = get_path(host);
    if (path == NULL) {
        return;
    }
    populate_opendir(host, path);
}


static void fs_readdir(zeal_hostfs_t *host)
{
    char out_name[ZOS_MAX_NAME_LENGTH];
    int desc = host->registers[2];

    hostfs_fd_t* fd = &host->descriptors[desc];
    if (desc >= MAX_OPENED_FILES || !descriptor_valid(fd)) {
        set_status(host, ZOS_FAILURE);
        return;
    }

    DIR *dir = fd->dir;
    struct dirent *entry = NULL;

    /* Look for a valid entry */
    while (1) {
        entry = readdir(dir);
        if (!entry) {
            set_status(host, ZOS_NO_MORE_ENTRIES);
            return;
        }
        /* Make sure to skip `.`, `..`, or special files */
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")
#ifndef _WIN32
        /* On Windows, files are either directories or files, no need to check */
            && (entry->d_type == DT_DIR || entry->d_type == DT_REG)
#endif
        ) {
            break;
        }
    }

    uint16_t buffer_addr = (host->registers[1] << 8) | host->registers[0];
    zos_format_name(entry->d_name, out_name);
    /* The structure to fill is defined in Zeal 8-bit OS
     * dir_entry_flags   DS.B 1  ; Is the entry a file ? A dir ?
     * dir_entry_name_t  DS.B 16 ; File name NULL-terminated, including the extension
     */
#ifdef _WIN32
    /* FIXME! Windows doens't have DT_REG flag */
    const uint8_t is_file = 1;
#else
    const uint8_t is_file = (entry->d_type == DT_REG) ? 1 : 0;
#endif
    memory_write_byte(host->host_ops, buffer_addr++, is_file);
    memory_write_bytes(host->host_ops, buffer_addr, (void*) out_name, ZOS_MAX_NAME_LENGTH);
    set_status(host, ZOS_SUCCESS);
}


static void handle_operation(zeal_hostfs_t *host, uint8_t operation)
{
    switch (operation) {
        case OP_WHOAMI:
            fs_whoami(host);
            break;
        case OP_OPEN:
            fs_open(host);
            break;
        case OP_CLOSE:
            fs_close(host);
            break;
        case OP_STAT:
            fs_stat(host);
            break;
        case OP_READ:
            fs_read(host);
            break;
        case OP_WRITE:
            fs_write(host);
            break;
        case OP_MKDIR:
            fs_mkdir(host);
            break;
        case OP_RM:
            fs_rm(host);
            break;
        case OP_OPENDIR:
            fs_opendir(host);
            break;
        case OP_READDIR:
            fs_readdir(host);
            break;
        default:
            set_status(host, ZOS_FAILURE);
            break;
    }
}


/**
 * Define the I/O device operations
 */
static uint8_t io_read(device_t* dev, uint32_t addr)
{
    zeal_hostfs_t* hostfs = (zeal_hostfs_t*) dev;
    return hostfs->registers[addr & 0xf];
}


static void io_write(device_t* dev, uint32_t addr, uint8_t value)
{
    zeal_hostfs_t* hostfs = (zeal_hostfs_t*) dev;
    /**
     * I/O mapping for write is as follows:
     *  0x00 - Start operation
     *      1 - Open directory
     *      2 - Read directory
     *      3 - Open file
     *      4 - Read file
     *      5 - Close
     *  0x01 - Path address, LSB
     *  0x02 - Path address, MSB
     */
    if (addr == OPERATION_REG) {
        if (value <= OP_LAST) {
            set_status(hostfs, ZOS_PENDING);
            handle_operation(hostfs, value);
        } else {
            log_err_printf("[HostFS] Invalid operation %x\n", value);
            set_status(hostfs, ZOS_FAILURE);
        }
    } else if (addr <= 7) {
        hostfs->registers[addr] = value;
    } else {
        log_err_printf("[HostFS] Unknown register 0x%x write\n", addr);
    }
}


int hostfs_init(zeal_hostfs_t* hostfs, const memory_op_t* ops)
{
    memset(hostfs, 0, sizeof(zeal_hostfs_t));
    hostfs->host_ops = ops;
    device_init_io(DEVICE(hostfs), "hostfs_dev", io_read, io_write, 0x10);
    return 0;
}


int hostfs_load_path(zeal_hostfs_t* hostfs, const char* root_path)
{
    char resolved_path_buffer[PATH_MAX];
    struct stat path_stat;

    if (root_path == NULL) {
        // assume the current working directory
        root_path = "./";
    }

    const char* resolved_path = NULL;
    resolved_path = realpath(root_path, resolved_path_buffer);

    if(resolved_path == NULL) {
        log_err_printf("[HostFS] Error resolving path %s\n", root_path);
        return 1;
    }

    if (stat(resolved_path, &path_stat) != 0) {
        log_err_printf("[HostFS] Path %s does not exist or is not accessible\n", path_sanitize(resolved_path));
        return 1;
    }

    if (!S_ISDIR(path_stat.st_mode)) {
        log_err_printf("[HostFS] Path %s must be a directory!\n", path_sanitize(resolved_path));
        return 1;
    }

    if (access(resolved_path, R_OK | W_OK) != 0) {
        log_err_printf("[HostFS] Path %s must be accessible in both read and write!\n", path_sanitize(resolved_path));
        return 1;
    }

    char* final_path = strdup(resolved_path);

#ifdef _WIN32
    for (char *p = final_path; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
#endif

    if (final_path == NULL) {
        log_err_printf("[HostFS] Could not allocate memory!\n");
        return 1;
    }
    hostfs->root_path = final_path;

    log_printf("[HostFS] %s loaded successfully\n", path_sanitize(get_relative_path(hostfs->root_path)));

    return 0;
}
