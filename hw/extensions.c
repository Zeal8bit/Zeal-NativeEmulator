#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "hw/zeal.h"
#include "utils/config.h"
#include "utils/log.h"

extern void zeal_add_io_device(zeal_t* machine, int region_start, device_t* dev);

// Define a wrapper function for zeal_add_io_device
int lua_zeal_add_io_device(lua_State* L) {
    zeal_t* machine = (zeal_t*)lua_touserdata(L, 1);
    int region_start = (int)lua_tointeger(L, 2);
    device_t* dev = (device_t*)lua_touserdata(L, 3);

    if (machine == NULL || dev == NULL) {
        lua_pushstring(L, "Invalid arguments to zeal_add_io_device");
        lua_error(L);
        return 0;
    }

    zeal_add_io_device(machine, region_start, dev);
    return 0;
}

int zeal_extensions_init(zeal_t* machine)
{
    // Register the zeal_add_io_device function in Lua
    lua_State* L = luaL_newstate();
    if (L == NULL) {
        log_printf("[EXT] Failed to create Lua state for function registration\n");
        return -1;
    }

    luaL_openlibs(L);

    /* Register the function in Lua */
    lua_register(L, "zeal_add_io_device", lua_zeal_add_io_device);

    // Load all "*.lua" files from the "extensions" directory
    FilePathList lua_files = LoadDirectoryFilesEx("extensions", ".lua", true);
    if (lua_files.count == 0) {
        log_printf("[EXT] No extension file found\n");
        return 0;
    }

    // Process loaded files (if needed)
    for (unsigned int i = 0; i < lua_files.count; i++) {
        printf("[EXT] Loading file: %s\n", lua_files.paths[i]);

        if (luaL_dofile(L, lua_files.paths[i]) != LUA_OK) {
            log_printf("[EXT] Error loading Lua file %s: %s\n", lua_files.paths[i], lua_tostring(L, -1));
            lua_close(L);
            continue;
        }

        lua_getglobal(L, "init");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                log_printf("[EXT] Error executing 'init' function in %s: %s\n", lua_files.paths[i], lua_tostring(L, -1));
            }
        } else {
            log_printf("[EXT] WARNING: No 'init' function found in %s\n", lua_files.paths[i]);
            lua_pop(L, 1);
        }

    }

    UnloadDirectoryFiles(lua_files);
    return 0;
}
