sources += files([
    'fifo.c',
    'paths.c',
    'files.c',
    'config.c'
])

if platform == 'web'
    linker_args += ['--js-library', meson.current_source_dir() + '/files_import_lib.js',
                    '-sASYNCIFY_IMPORTS=[files_import_js]'
                   ]
endif
