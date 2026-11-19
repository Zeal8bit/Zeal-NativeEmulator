common_sources = files(
    'app.c',
    'config_args.c',
    'flash.c',
    'host.c',
    'media.c',
    'paths.c',
    'rtc.c',
)
if get_option('enable_debugger')
    common_sources += files('symbols.c')
endif
graphical_cli = files('graphical_cli.c')
