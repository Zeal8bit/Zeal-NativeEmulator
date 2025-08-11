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
dependencies = []
OUTPUT_BIN = 'zeal.elf'

# Retrieve raylib_path option (set via command line or use system defaults)
raylib_path = get_option('raylib_path')
raylib_target_path = meson.get_external_property('raylib_path', '')

if raylib_path != ''
    # Target provided path
    raylib_include_dir = raylib_path / 'include'
    raylib_lib_dir = raylib_path / 'lib'
elif raylib_target_path != ''
    # Target provided path
    raylib_include_dir = raylib_target_path / 'include'
    raylib_lib_dir = raylib_target_path / 'lib'
else
    # No path provided, rely on system's default libraries (i.e., /usr/include and /usr/lib)
    raylib_include_dir = '/usr/include'
    raylib_lib_dir = '/usr/lib'
endif


# List all the subdirectories that contain source code
sources = []
subdir('hw')
subdir('utils')
subdir('assets')

run_command('mkdir', '-p', join_paths(meson.project_build_root(), 'roms'), check: true)
run_command(
    'cp',
    'roms/default.img',
    join_paths(meson.project_build_root(), 'roms/default.img'),
    check: true,
)

if (get_option('show_fps'))
    add_project_arguments('-DSHOW_FPS', language: 'c')
endif

add_project_arguments('-D_POSIX_C_SOURCE=200809L',
                      '-D_DARWIN_C_SOURCE',
                      '-Iinclude',
                      language: 'c')
add_project_arguments('-I' + raylib_include_dir, language: 'c')
add_project_link_arguments('-L' + raylib_lib_dir, '-lraylib', '-lm', language: 'c')

linker_args = meson.get_external_property('link_args', [])

# Compiler and build targets
exec = executable(
    OUTPUT_BIN,
    sources,
    include_directories: ['include'],
    dependencies: dependencies,
    link_args: linker_args,
)

# meson compile run to launch emulator after building
run_target(
    'run',
    command: [exec],
)

# We have to do it here so that the final file is at the root of the build dir
if (host_machine.system() == 'emscripten')
    configure_file(
        input: 'assets/index.html',
        output: 'index.html',
        copy: true
    )
    configure_file(
        input: 'assets/index.css',
        output: 'index.css',
        copy: true
    )

    patch_script = join_paths(meson.source_root(), 'tools', 'wasm_patch.py')

    patch_wasm = custom_target('patch_wasm',
        output: 'dummy_file',
        command: ['python3', patch_script, OUTPUT_BIN + '.js'],
        depends: exec,
        build_by_default: true,
    )

endif

