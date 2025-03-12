project(
    'Zeal-NativeEmulator',
    'c',
    version: '1.0',
    license: 'Apache-2.0',
    default_options: [
        'buildtype=debugoptimized',
        'c_std=c99',
        'warning_level=everything',
        'warning_level=2',
    ],
)


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

run_command('mkdir', '-p', join_paths(meson.project_build_root(), 'roms'), check: true)
run_command(
    'cp',
    'roms/default.img',
    join_paths(meson.project_build_root(), 'roms/default.img'),
    check: true,
)

add_project_arguments('-D_POS_C_SOURCE=200809L', '-D_DARWIN_C_SOURCE', '-Iinclude', language: 'c')
add_project_arguments('-I' + raylib_include_dir, language: 'c')
add_project_link_arguments('-L' + raylib_lib_dir, '-lraylib', '-lm', language: 'c')


# Compiler and build targets
exec = executable(
    OUTPUT_BIN,
    sources,
    include_directories: ['include'],
    dependencies: [],
)

# meson compile run to launch emulator after building
custom_target(
    'run',
    depends: exec,
    input: exec,
    output: 'fake',
    command: ['./' + OUTPUT_BIN],
    build_by_default: false,
)
