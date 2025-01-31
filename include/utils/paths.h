#include <stdint.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define PATH_MAX    _MAX_PATH
#elif __linux__
    #include <unistd.h>
    #include <limits.h>
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <limits.h>
#endif


void get_executable_path(char *buffer, size_t size);
void get_executable_dir(char *buffer, size_t size);

int get_install_dir_file(char dst[PATH_MAX], const char* name);
