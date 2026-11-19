common_sources = files(
    'app.c',
    'config_args.c',
    'config_file.c',
    'flash.c',
    'host.c',
    'media.c',
    'paths.c',
    'rtc.c',
)
ini_sources = files('config.c')
if get_option('enable_debugger')
    common_sources += files('symbols.c')
endif
