common_sources = files(
    'app.c',
    'flash.c',
    'host.c',
    'media.c',
    'paths.c',
    'rtc.c',
)
if debugger_api
    common_sources += files('symbols.c')
endif

# SDL3 playback shared by the graphical frontends.
common_audio_sources = files('audio.c')

video_inspect_sources = files('video_inspect.c')
