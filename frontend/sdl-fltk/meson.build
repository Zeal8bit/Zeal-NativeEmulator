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
sdlfltk_ui_inc = include_directories('ui')
# UI: app/ (main window), theme/, widgets/ and common/ are always built;
# the debugger UI directories only with the debugger. Without it,
# app/debugger_ui_disabled.cpp stands in for the debugger.
sdlfltk_cpp_sources = files(
    'ui/app/app_menus.cpp',
    'ui/app/keymap.cpp',
    'ui/app/main_window.cpp',
    'ui/app/notifications.cpp',
    'ui/app/screen_view.cpp',
    'ui/app/ui_entry.cpp',
    'ui/common/clock.cpp',
    'ui/common/cp437.cpp',
    'ui/common/format.cpp',
    'ui/common/persist.cpp',
    'ui/theme/fonts.cpp',
    'ui/theme/style.cpp',
    'ui/theme/theme.cpp',
    'ui/widgets/dock_layout.cpp',
    'ui/widgets/icon_button.cpp',
    'ui/widgets/inline_edit.cpp',
    'ui/widgets/panel.cpp',
    'ui/widgets/split_pane.cpp',
)
if get_option('enable_debugger')
    sdlfltk_cpp_sources += files(
        'ui/debugger/address_lookup.cpp',
        'ui/debugger/breakpoint_list.cpp',
        'ui/debugger/breakpoint_marker.cpp',
        'ui/debugger/breakpoints_panel.cpp',
        'ui/debugger/debugger_view.cpp',
        'ui/debugger/disasm_panel.cpp',
        'ui/debugger/disasm_view.cpp',
        'ui/debugger/layout_config.cpp',
        'ui/debugger/machine_state.cpp',
        'ui/debugger/mem_find_input.cpp',
        'ui/debugger/mem_view.cpp',
        'ui/debugger/memory_panel.cpp',
        'ui/debugger/mmu_strip.cpp',
        'ui/debugger/navigation.cpp',
        'ui/debugger/registers_view.cpp',
        'ui/debugger/stack_view.cpp',
        'ui/debugger/state_pill.cpp',
        'ui/debugger/toolbar.cpp',
        'ui/debugger/watch_list.cpp',
        'ui/debugger/watch_model.cpp',
        'ui/debugger/watch_panel.cpp',
        'ui/tools/tool_registry.cpp',
        'ui/tools/tool_tab.cpp',
        'ui/tools/tool_window.cpp',
        'ui/tools/tool_windows.cpp',
        'ui/memory_tools.cpp',
        'ui/video.cpp',
        'ui/view_console.cpp',
        'ui/view_hotcode.cpp',
        'ui/view_hw_io.cpp',
        'ui/view_hw_sound.cpp',
        'ui/view_hw_video.cpp',
        'ui/view_input.cpp',
        'ui/view_semihost.cpp',
        'ui/view_symbols.cpp',
    )
else
    sdlfltk_cpp_sources += files('ui/app/debugger_ui_disabled.cpp')
endif
