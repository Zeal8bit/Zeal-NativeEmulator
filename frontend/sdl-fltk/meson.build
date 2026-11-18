# SDL3 (audio, gamepads) + FLTK (windows, menus, input, debugger UI).
#
# Optional parts are whole files picked here, never #if gates in the code:
# debugger-only sources (debug/, most of ui/) or their *_disabled.c stubs,
# and the render profiler or its stub.
sdlfltk_sources = files(
    'bridge/cpu.c',
    'bridge/hw.c',
    'bridge/input.c',
    'bridge/machine.c',
    'machine/audio.c',
    'machine/controller.c',
    'machine/frontend.c',
    'machine/keyboard.c',
    'machine/mouse.c',
    'machine/snes_adapter.c',
)
if get_option('enable_debugger')
    sdlfltk_sources += files(
        'debug/console.c',
        'debug/debug.c',
        'debug/pcprof.c',
        'debug/sampler.c',
        'debug/symbols.c',
        'debug/vram.c',
    )
else
    sdlfltk_sources += files('debug_disabled.c')
endif
if get_option('profile_render')
    sdlfltk_sources += files('profile/render_profile.c')
else
    sdlfltk_sources += files('profile/profile_disabled.c')
endif
sdlfltk_cpp_sources = files(
    'ui/app.cpp',
    'ui/common.cpp',
    'ui/debugger.cpp',
    'ui/memory_tools.cpp',
    'ui/screen.cpp',
    'ui/theme.cpp',
    'ui/tool_window.cpp',
    'ui/video.cpp',
    'ui/view_console.cpp',
    'ui/view_hotcode.cpp',
    'ui/view_hw_io.cpp',
    'ui/view_hw_sound.cpp',
    'ui/view_hw_video.cpp',
    'ui/view_input.cpp',
    'ui/view_semihost.cpp',
    'ui/view_symbols.cpp',
    'ui/view_watch.cpp',
)
