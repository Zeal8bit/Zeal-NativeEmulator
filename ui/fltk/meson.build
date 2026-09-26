# Cross builds use target pkg-config metadata, never the host fltk-config.
fltk_dep = dependency('fltk', version: '>=1.4', method: 'pkg-config', required: false)
if not fltk_dep.found() and not meson.is_cross_build()
    fltk_config = find_program('fltk-config')
    fltk_version = run_command(fltk_config, '--version', check: true).stdout().strip()
    if not fltk_version.version_compare('>=1.4')
        error('FLTK 1.4 or newer is required')
    endif
    fltk_dep = declare_dependency(
        compile_args: run_command(fltk_config, '--cxxflags', check: true).stdout().strip().split(),
        link_args: run_command(fltk_config, '--ldflags', check: true).stdout().strip().split())
elif not fltk_dep.found()
    error('Cross builds require target FLTK 1.4 pkg-config metadata')
endif
dependencies += fltk_dep
sources += files('frontend.cpp', 'theme.cpp', 'dock.cpp')

if get_option('fltk_test_hooks')
    add_project_arguments('-DCONFIG_FLTK_TESTS=1', language: 'cpp')
endif
