# SDL3 (audio, gamepads) + FLTK (windows, menus, input, debugger UI).
sdlfltk_sources = files(
    'audio.c',
    'bridge.c',
    'controller.c',
    'frontend.c',
    'keyboard.c',
    'mouse.c',
    'profile.c',
    'snes_adapter.c',
)
sdlfltk_cpp_sources = files(
    'ui/app.cpp',
    'ui/debugger.cpp',
    'ui/screen.cpp',
)
if get_option('enable_debugger')
    sdlfltk_sources += files('debug_video.c')
endif
