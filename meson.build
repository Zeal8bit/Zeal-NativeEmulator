project(
    'Zeal-NativeEmulator',
    'c',
    version: '1.0',
    license: 'Apache-2.0',
    default_options: [
        'buildtype=debugoptimized',
        'c_std=c99',
        'cpp_std=c++17',
        'warning_level=everything',
        'warning_level=2',
    ],
)


# Output binary
dependencies = []
OUTPUT_BIN = 'zeal-native'
if (host_machine.system() == 'emscripten')
    OUTPUT_BIN = 'zeal.elf'
endif

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
    raylib_include_dir = ''
    raylib_lib_dir = ''
endif


ui_option = get_option('debugger_ui')
is_web = host_machine.system() == 'emscripten'
if ui_option == 'fltk' and (is_web or not get_option('enable_debugger'))
    error('FLTK requires a desktop build with enable_debugger=true')
endif
fltk_ui = get_option('enable_debugger') and not is_web and ui_option != 'none'
if fltk_ui
    add_languages('cpp', native: false, required: true)
    add_project_arguments('-DCONFIG_FLTK_UI=1', language: 'c')
endif

# Which video blitter to build. The software blitter renders into CPU memory, which is
# what lets a desktop build drop Raylib entirely, so it is the default there; the web
# build keeps the GPU shader.
blitter_option = get_option('blitter')
if blitter_option == 'auto'
    software_blitter = fltk_ui
else
    software_blitter = blitter_option == 'software'
endif

# List all the subdirectories that contain source code
sources = []
subdir('host')
subdir('hw')
subdir('app')
subdir('utils')
subdir('assets')
if fltk_ui
    subdir('ui/fltk')
endif


run_command('mkdir', '-p', join_paths(meson.project_build_root(), 'roms'), check: true)
res = run_command(
    'cp',
    'roms/default.img',
    join_paths(meson.project_build_root(), 'roms/default.img'),
    check: false,
)

if res.returncode() != 0
  warning('default.img not found, skipping copy')
endif


if (get_option('show_fps'))
    add_project_arguments('-DCONFIG_SHOW_FPS', language: 'c')
endif

if get_option('profile_render')
    add_project_arguments('-DCONFIG_PROFILE_RENDER', language: 'c')
endif

if get_option('enable_debugger')
    add_project_arguments('-DCONFIG_ENABLE_DEBUGGER', language: 'c')
endif

if (host_machine.system() == 'darwin')
    add_project_arguments('-D_DARWIN_C_SOURCE', language: 'c')
endif

add_project_arguments('-D_XOPEN_SOURCE=700',
                      '-Iinclude',
                      language: 'c')

if software_blitter
    add_project_arguments('-DZVB_BLITTER_SOFTWARE=1', language: 'c')
    if (get_option('blitter_software_scanline_rendering'))
        add_project_arguments('-DZVB_BLITTER_SOFTWARE_SCANLINE_RENDERING=1', language: 'c')
    endif
else
    add_project_arguments('-DZVB_BLITTER_SHADER=1', language: 'c')
    if (get_option('blitter_software_scanline_rendering'))
        warning('blitter_software_scanline_rendering requires the software blitter, ignoring')
    endif
endif

zeal_assets_dir = join_paths(get_option('prefix'), get_option('datadir'), 'zeal-native', 'assets')
add_project_arguments('-DZEAL_ASSETS_DIR="' + zeal_assets_dir + '"', language: 'c')

if raylib_include_dir != ''
    add_project_arguments('-I' + raylib_include_dir, language: 'c')
endif

if raylib_lib_dir != ''
    add_project_link_arguments('-L' + raylib_lib_dir, language: 'c')
endif

add_project_link_arguments('-lraylib', '-lm', language: 'c')

if host_machine.system() == 'linux'
    add_project_link_arguments('-lX11', language: 'c')
endif

linker_args = meson.get_external_property('link_args', [])

if host_machine.system() == 'emscripten'
    if get_option('wasm_embed_roms')
        linker_args += ['--embed-file', 'roms']
    endif
endif

# FLTK flags and includes apply only to the C++ frontend library.
ui_objects = []
ui_link_dependencies = []
if fltk_ui
    ui_library = static_library('zeal-fltk-ui', fltk_sources, build_by_default: false,
        include_directories: ['include'], dependencies: [fltk_dep] + host_fltk_deps, override_options: ['cpp_std=c++17'])
    ui_objects = ui_library.extract_all_objects(recursive: true)
    ui_link_dependencies += fltk_dep.partial_dependency(link_args: true, links: true)
    # Library-only dependencies of the UI library must reach the link too, since the
    # executable links the extracted objects rather than the archive.
    ui_link_dependencies += host_fltk_deps
endif

# Compiler and build targets
exec = executable(
    OUTPUT_BIN,
    sources, files('hw/main.c'),
    include_directories: ['include', '.'],
    dependencies: dependencies + ui_link_dependencies + host_deps,
    objects: ui_objects,
    link_language: fltk_ui ? 'cpp' : 'c',
    link_args: linker_args + ['-lraylib', '-lm'] + (raylib_lib_dir != '' ? ['-L' + raylib_lib_dir] : []),
    install: true
)

# meson compile run to launch emulator after building
run_target(
    'run',
    command: [exec],
)

# We have to do it here so that the final file is at the root of the build dir
if (host_machine.system() == 'emscripten')
    wasm_template = get_option('wasm_template')
    wasm_dir = join_paths(meson.project_source_root(), 'assets', 'wasm', wasm_template)
    configure_file(
        input: join_paths(wasm_dir, 'index.html'),
        output: 'index.html',
        copy: true
    )
    configure_file(
        input: join_paths(wasm_dir, 'index.js'),
        output: 'index.js',
        copy: true
    )
    configure_file(
        input: join_paths(wasm_dir, 'index.css'),
        output: 'index.css',
        copy: true
    )
    if wasm_template == 'native'
        configure_file(
            input: join_paths(wasm_dir, 'favicon.ico'),
            output: 'favicon.ico',
            copy: true
        )
    endif

    wasm_lib_sources = files(
        'assets/wasm/libnative/indexeddb.js',
        'assets/wasm/libnative/disk-images.js',
        'assets/wasm/libnative/hostfs.js',
        'assets/wasm/libnative/zeal-native.js',
    )
    wasm_lib_copy_script = join_paths(
        meson.project_source_root(),
        'tools',
        'copy_wasm_lib.py',
    )
    custom_target(
        'copy_wasm_libnative',
        input: wasm_lib_sources,
        output: 'libnative.stamp',
        command: [
            'python3',
            wasm_lib_copy_script,
            join_paths(meson.project_build_root(), 'libnative'),
            '@OUTPUT@',
            '@INPUT@',
        ],
        build_by_default: true,
    )

    patch_script = join_paths(meson.project_source_root(), 'tools', 'wasm_patch.py')

    patch_wasm = custom_target('patch_wasm',
        output: 'dummy_file',
        command: ['python3', patch_script, OUTPUT_BIN + '.js'],
        depends: exec,
        build_by_default: true,
    )
endif

if get_option('enable_debugger')
    api_test = executable('debugger-api-test',
        'tests/debugger_api.c', 'hw/debugger/debugger.c', 'hw/debugger/api.c', 'hw/debugger/bindings.c',
        include_directories: ['include', '.'],
        link_args: is_web ? ['-sMODULARIZE=1', '-sEXPORTED_RUNTIME_METHODS=HEAPU8', '-sEXPORTED_FUNCTIONS=_main,_malloc,_free'] : [])
    if is_web
        test('debugger-wasm-exports', find_program('node'), args: [files('tests/wasm_debugger.cjs'), api_test.full_path()], depends: api_test)
    else
        test('debugger-api', api_test)
    endif
endif

if fltk_ui
    models_test = executable('fltk-models-test', 'tests/fltk_models.cpp', 'ui/fltk/theme.cpp', 'ui/fltk/dock.cpp', 'ui/fltk/input.cpp', include_directories: ['.', 'include'], dependencies: fltk_dep, override_options: ['cpp_std=c++17'])
    test('fltk-models', models_test)
endif

if get_option('enable_debugger') and not fltk_ui and not is_web
    machine_test = executable('debugger-machine-test', sources, 'tests/debugger_machine.c',
        include_directories: ['include', '.'], dependencies: dependencies, link_args: linker_args)
    test('debugger-machine', machine_test, env: ['ZEAL_CONFIG_DIR=' + meson.project_build_root() / 'test-config'])
endif
