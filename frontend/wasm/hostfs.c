/*
 * SPDX-FileCopyrightText: 2025 Zeal 8-bit Computer <contact@zeal8bit.com>; David Higgins <zoul0813@me.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <emscripten.h>

#include "frontend/common/hostfs.h"
#include "hw/mmu.h"

/**
 * @brief Macros related to Zeal 8-bit OS public API
 */
#define ZOS_FAILURE 1
#define ZOS_PENDING 0xFF

/**
 * @brief Operations for the Host FS
 */
#define OP_WHOAMI 0
#define OP_OPEN 1
#define OP_STAT 2
#define OP_READ 3
#define OP_WRITE 4
#define OP_CLOSE 5
#define OP_OPENDIR 6
#define OP_READDIR 7
#define OP_MKDIR 8
#define OP_RM 9

#define OPERATION_REG 0xF

#define ZOS_FD_OFFSET_T (8)
#define ZOS_FD_USER_T (12)

static void set_status(zeal_hostfs_t* host, uint8_t status)
{
    host->registers[OPERATION_REG] = status;
}

static void fs_whoami(zeal_hostfs_t* host)
{
    set_status(host, 0xD3);
}

static zeal_hostfs_t* web_hostfs;

// clang-format off
EM_JS(int, hostfs_web_enabled, (), {
    return Module.hostfsBackend ? 1 : 0;
});

EM_JS(void, hostfs_web_start, (int operation, const char* path, int flags, int descriptor, uint32_t offset, uint16_t guest_address, uint16_t length, const uint8_t* write_data), {
    const request = {
        operation,
        path: path ? UTF8ToString(path) : null,
        flags,
        descriptor,
        offset: offset >>> 0,
        guestAddress: guest_address,
        length,
        data: write_data ? HEAPU8.slice(write_data, write_data + length) : null,
    };
    Module.hostfsBackend.start(request);
});
// clang-format on

EMSCRIPTEN_KEEPALIVE
void hostfs_web_complete(uint8_t status, uint8_t reg0, uint8_t reg1,
                         uint8_t reg2, uint8_t reg3, uint8_t reg4, uint8_t reg5)
{
    if (web_hostfs == NULL || web_hostfs->registers[OPERATION_REG] != ZOS_PENDING) {
        return;
    }
    web_hostfs->registers[0] = reg0;
    web_hostfs->registers[1] = reg1;
    web_hostfs->registers[2] = reg2;
    web_hostfs->registers[3] = reg3;
    web_hostfs->registers[4] = reg4;
    web_hostfs->registers[5] = reg5;
    set_status(web_hostfs, status);
}

EMSCRIPTEN_KEEPALIVE
void hostfs_web_write_guest(uint16_t address, uint8_t* data, uint16_t length)
{
    if (web_hostfs != NULL && data != NULL) {
        mmu_virt_write_array(web_hostfs->mmu, address, data, length);
    }
}

static char* get_web_path(zeal_hostfs_t* host)
{
    uint16_t address = (host->registers[2] << 8) | host->registers[1];
    char* path = calloc(1, 256);
    if (path == NULL) return NULL;

    size_t i;
    for (i = 0; i < 255; i++) {
        path[i] = (char) mmu_virt_read_byte(host->mmu, address++);
        if (path[i] == '\0') return path;
    }
    path[255] = '\0';
    return path;
}

static void handle_web_operation(zeal_hostfs_t* host, uint8_t operation)
{
    char* path = NULL;
    uint8_t* data = NULL;
    uint8_t flags = host->registers[0];
    int descriptor = -1;
    uint32_t offset = 0;
    uint16_t guest_address = 0;
    uint16_t length = 0;

    if (!hostfs_web_enabled()) {
        set_status(host, ZOS_FAILURE);
        return;
    }
    if (operation == OP_WHOAMI) {
        fs_whoami(host);
        return;
    }
    if (operation == OP_OPEN || operation == OP_OPENDIR ||
        operation == OP_MKDIR || operation == OP_RM) {
        path = get_web_path(host);
        if (path == NULL) {
            set_status(host, ZOS_FAILURE);
            return;
        }
    } else if (operation == OP_CLOSE) {
        descriptor = host->registers[0];
    } else if (operation == OP_STAT || operation == OP_READDIR) {
        guest_address = (host->registers[1] << 8) | host->registers[0];
        descriptor = host->registers[2];
    } else if (operation == OP_READ || operation == OP_WRITE) {
        const uint16_t structure = (host->registers[1] << 8) | host->registers[0];
        uint8_t raw_offset[4];
        descriptor = mmu_virt_read_byte(host->mmu, structure + ZOS_FD_USER_T);
        mmu_virt_read_array(host->mmu, structure + ZOS_FD_OFFSET_T, raw_offset, sizeof(raw_offset));
        offset        = (uint32_t) raw_offset[0] |
                        ((uint32_t) raw_offset[1] << 8) |
                        ((uint32_t) raw_offset[2] << 16) |
                        ((uint32_t) raw_offset[3] << 24);
        guest_address = (host->registers[3] << 8) | host->registers[2];
        length = (host->registers[5] << 8) | host->registers[4];
        if (operation == OP_WRITE && length != 0) {
            data = malloc(length);
            if (data == NULL) {
                set_status(host, ZOS_FAILURE);
                return;
            }
            mmu_virt_read_array(host->mmu, guest_address, data, length);
        }
    }

    hostfs_web_start(operation, path, flags, descriptor, offset,
                     guest_address, length, data);
    free(data);
    free(path);
}

static void frontend_hostfs_dispatch(void* userdata, uint8_t operation)
{
    handle_web_operation(userdata, operation);
}

void frontend_hostfs_bind(zeal_hostfs_t* core)
{
    core->userdata = core;
    core->dispatch = frontend_hostfs_dispatch;
    core->guard_pending = true;
    web_hostfs = core;
}

void frontend_hostfs_close(void)
{
    if (web_hostfs) {
        web_hostfs->dispatch = NULL;
        web_hostfs->userdata = NULL;
        web_hostfs = NULL;
    }
}
