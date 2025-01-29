#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>  // For dirname on Linux/macOS

#ifdef _WIN32
#define PATH_MAX    _MAX_PATH
#endif

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
#else
    #error "Unsupported platform"
#endif

void get_executable_dir(char *buffer, size_t size) {
    get_executable_path(buffer, size);
    strcpy(buffer, dirname(buffer));  // Get the directory part of the path
}
