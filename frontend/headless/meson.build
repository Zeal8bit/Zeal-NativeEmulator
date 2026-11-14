headless_sources = files(
    'console.c',
    'console_keys.c',
    'frontend.c',
)
if get_option('enable_debugger')
    headless_sources += files('console_debug.c')
endif
