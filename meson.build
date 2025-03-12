project('Zeal-NativeEmulator', 'c', version : '1.0', license : 'Apache-2.0')

# Output binary
OUTPUT_BIN = 'zeal.elf'

# Retrieve raylib_path option (set via command line or use system defaults)
raylib_path = get_option('raylib_path')
if raylib_path == ''
  # No path provided, rely on system's default libraries (i.e., /usr/include and /usr/lib)
  raylib_include_dir = '/usr/include'
  raylib_lib_dir = '/usr/lib'
else
  # Use the provided path
  raylib_include_dir = raylib_path / 'include'
  raylib_lib_dir = raylib_path / 'lib'
endif

# List all the subdirectories that contain source code
sources = []
subdir('hw')
subdir('utils')

# Compiler flags
cflags = [
  '-g', '-Wall', '-Wextra', '-O2', '-std=c99',
  '-D_POSIX_C_SOURCE=200809L', '-D_DARWIN_C_SOURCE',
  '-Iinclude/', '-I' + raylib_include_dir
]

# Linker flags
ldflags = [
  '-L' + raylib_lib_dir, '-lraylib', '-lm'
]

# Compiler and build targets
executable(OUTPUT_BIN, sources,
            include_directories : ['include'],
            dependencies : [],
            c_args : cflags,
            link_args : ldflags)
