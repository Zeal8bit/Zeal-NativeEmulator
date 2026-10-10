# An explicitly supplied fltk-config must describe the target, not the build host.
fltk_config_path = get_option('fltk_config')
if fltk_config_path == ''
    fltk_config_path = meson.get_external_property('fltk_config', '')
endif
fltk_dep = dependency('fltk', version: '>=1.4', method: 'pkg-config', required: false)
if fltk_config_path != '' or (not fltk_dep.found() and not meson.is_cross_build())
    fltk_config = find_program(fltk_config_path != '' ? fltk_config_path : 'fltk-config')
    fltk_version = run_command(fltk_config, '--version', check: true).stdout().strip()
    if not fltk_version.version_compare('>=1.4')
        error('FLTK 1.4 or newer is required')
    endif
    fltk_dep = declare_dependency(
        compile_args: run_command(fltk_config, '--cxxflags', check: true).stdout().strip().split(),
        link_args: run_command(fltk_config, '--ldflags', check: true).stdout().strip().split())
elif not fltk_dep.found()
    error('Cross builds require target FLTK 1.4 pkg-config metadata or -Dfltk_config=/path/to/target/fltk-config')
endif
fltk_sources = files(
    'frontend.cpp',
    'theme.cpp',
    'dock.cpp',
    'input.cpp',
    'util.cpp',
    'debugger_ui.cpp',
    'widgets/dock_surface.cpp',
    'widgets/float_window.cpp',
    'widgets/table.cpp',
    'widgets/text_input.cpp',
    'widgets/canvas.cpp',
    'widgets/control_row.cpp',
    'widgets/memory_table.cpp',
    'widgets/list_table.cpp',
    'panels/panel.cpp',
    'panels/video.cpp',
    'panels/cpu.cpp',
    'panels/breakpoints.cpp',
    'panels/disassembler.cpp',
    'panels/memory.cpp',
    'panels/mmu.cpp',
    'panels/semihost.cpp',
    'panels/vram.cpp',
)

# The desktop host backend needs FLTK's compile flags, so it is built here
# rather than with the other host sources. It rasterizes the bundled CP437 font
# with FreeType.
host_fltk_deps = []
if software_blitter
    fltk_sources += files('../../host/zeal_host_fltk.cpp')
    host_fltk_deps += dependency('freetype2')
endif

if get_option('fltk_test_hooks')
    add_project_arguments('-DCONFIG_FLTK_TESTS=1', language: 'cpp')
    fltk_sources += files('smoke.cpp', '../../tests/fltk_popup_checks.cpp')
endif
