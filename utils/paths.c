#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>  // For dirname on Linux/macOS
#include <sys/stat.h>
#include "utils/paths.h"
#include "utils/log.h"

static char path_buffer[PATH_MAX] = { 0 };

#ifdef _WIN32
    #include <windows.h>
    void get_executable_path(char *buffer, size_t size) {
        GetModuleFileName(NULL, buffer, (DWORD)size);
    }
#elif __linux__
    #include <unistd.h>
    #include <limits.h>
    void get_executable_path(char *buffer, size_t size) {
        ssize_t len = readlink("/proc/self/exe", buffer, size - 1);
        if (len != -1) buffer[len] = '\0';
    }
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
    void get_executable_path(char *buffer, size_t size) {
        uint32_t bufsize = (uint32_t)size;
        if (_NSGetExecutablePath(buffer, &bufsize) != 0) {
            fprintf(stderr, "Buffer too small; need size %u\n", bufsize);
        }
    }
#elif PLATFORM_WEB
    void get_executable_path(char *buffer, size_t size) {
        if (size > 1) {
            buffer[0] = '/';
            buffer[1] = 0;
        }
    }
#else
    #error "Unsupported platform"
#endif

void get_executable_dir(char *buffer, size_t size) {
    get_executable_path(buffer, size);
    const char* dir = dirname(buffer);
    /* strcpy doesn't handle overlapping pointers properly */
    if(strlen(dir) + 1 > size) return; // too big
    memmove(buffer, dir, strlen(dir) + 1);
}


int get_install_dir_file(char dst[PATH_MAX], const char* name) {
    /* Only get it once */
    if (path_buffer[0] == 0) {
        get_executable_dir(path_buffer, PATH_MAX);
    }
    const int wrote = snprintf(dst, PATH_MAX, "%s/%s", path_buffer, name);
    return wrote < PATH_MAX;
}

const char* get_shaders_path(char dst[PATH_MAX], const char* name)
{
    const char* assets_dir = "assets/shaders/";
    if (path_buffer[0] == 0) {
        get_executable_dir(path_buffer, PATH_MAX);
    }
    int written = snprintf(dst, PATH_MAX, "%s/%s/%s", path_buffer, assets_dir, name);
    if (written == PATH_MAX) {
        log_err_printf("Could not load shader %s, path is too long!\n", name);
        return NULL;
    }
    return dst;
}

int path_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

char* get_relative_path(const char* absolute_path)
{
    static char relative_path[PATH_MAX];
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "Could not get current working directory: %s\n", strerror(errno));
        return NULL;
    }

    char abs_cwd[PATH_MAX];
    char abs_path[PATH_MAX];

    if (realpath(cwd, abs_cwd) == NULL || realpath(absolute_path, abs_path) == NULL) {
        fprintf(stderr, "Could not resolve paths: %s\n", strerror(errno));
        return NULL;
    }

    const char* cwd_ptr = abs_cwd;
    const char* path_ptr = abs_path;

    // Find the common prefix
    size_t common_prefix_len = strspn(cwd_ptr, path_ptr);
    cwd_ptr += common_prefix_len;
    path_ptr += common_prefix_len;

    // Count the remaining directories in cwd
    int up_levels = 0;
    for (const char* p = cwd_ptr; *p != '\0'; p++) {
        if (*p == '/') {
            up_levels++;
        }
    }

    // Construct the relative path
    char* rel_ptr = relative_path;
    for (int i = 0; i < up_levels; i++) {
        strcpy(rel_ptr, "../");
        rel_ptr += 3;
    }
    strcpy(rel_ptr, path_ptr);


    return relative_path;
}
