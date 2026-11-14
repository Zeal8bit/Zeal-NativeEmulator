project('Zeal-NativeEmulator', 'c', version: '1.0.1', license: 'Apache-2.0',
    default_options: ['buildtype=debugoptimized', 'c_std=c99', 'warning_level=2'])

cc = meson.get_compiler('c')
frontend = get_option('frontend')
is_web = host_machine.system() == 'emscripten'
if frontend == 'auto'
    frontend = is_web ? 'nuklear' : 'both'
endif
build_nuklear = frontend == 'nuklear' or frontend == 'both'
build_headless = frontend == 'headless' or frontend == 'both'

core_args = []
if get_option('enable_debugger')
    core_args += '-DCONFIG_ENABLE_DEBUGGER=1'
endif
if get_option('scanline_rendering')
    core_args += '-DZVB_SCANLINE_RENDERING=1'
endif
if get_option('profile_render')
    core_args += '-DCONFIG_PROFILE_RENDER=1'
endif
includes = include_directories('include')
portable_deps = [cc.find_library('m', required: false)]
sources = []
subdir('hw')
subdir('utils')
core = static_library('zeal', sources, include_directories: includes,
    c_args: core_args, dependencies: portable_deps)
zeal_dep = declare_dependency(link_with: core, include_directories: includes,
    compile_args: core_args, dependencies: portable_deps)

if build_nuklear or build_headless
    subdir('frontend/common')
    host_args = ['-D_XOPEN_SOURCE=700',
        '-DZEAL_NATIVE_VERSION="@0@"'.format(meson.project_version())]
    if host_machine.system() == 'darwin'
        host_args += '-D_DARWIN_C_SOURCE'
    endif
    if is_web
        host_args += '-DPLATFORM_WEB'
    endif
endif

if build_headless
    subdir('frontend/headless')
    executable('zeal-headless', common_sources + headless_sources,
        dependencies: zeal_dep, c_args: host_args + ['-DFRONTEND_HEADLESS=1'], install: true)
endif

if build_nuklear
    subdir('frontend/nuklear')
    subdir('assets')
    raylib_path = get_option('raylib_path')
    if raylib_path == ''
        raylib_path = meson.get_external_property('raylib_path', '')
    endif
    graphics_args = ['-DFRONTEND_HEADLESS=0']
    graphics_link = ['-lraylib']
    if raylib_path != ''
        graphics_args += '-I' + raylib_path / 'include'
        graphics_link += '-L' + raylib_path / 'lib'
    endif
    if host_machine.system() == 'linux'
        graphics_link += '-lX11'
    endif
    if get_option('show_fps')
        graphics_args += '-DCONFIG_SHOW_FPS=1'
    endif
    zeal_assets_dir = get_option('prefix') / get_option('datadir') / 'zeal-native/assets'
    graphics_args += '-DZEAL_ASSETS_DIR="' + zeal_assets_dir + '"'
    linker_args = meson.get_external_property('link_args', [])
    if is_web and get_option('wasm_embed_roms')
        linker_args += ['--embed-file', 'roms']
    endif
    OUTPUT_BIN = is_web ? 'zeal.elf' : 'zeal-native'
    exec = executable(OUTPUT_BIN, common_sources + nuklear_sources,
        dependencies: zeal_dep, c_args: host_args + graphics_args,
        link_args: graphics_link + linker_args, install: true)
    run_target('run', command: [exec])
    run_command('mkdir', '-p', meson.project_build_root() / 'roms', check: true)
    res = run_command('cp', 'roms/default.img', meson.project_build_root() / 'roms/default.img', check: false)
    if res.returncode() != 0
        warning('default.img not found, skipping copy')
    endif
endif

# We have to do it here so that the final file is at the root of the build dir
if is_web and build_nuklear
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
