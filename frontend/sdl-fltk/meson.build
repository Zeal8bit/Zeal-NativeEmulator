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
# UI (C++), one class per file, directory = namespace (zui::<dir>).
# app/, common/, theme/ and widgets/ are always built; the debugger UI
# directories only with the debugger. Without it, app/debugger_ui_disabled.cpp
# stands in for the debugger.
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
    'ui/widgets/filter_input.cpp',
    'ui/widgets/icon_button.cpp',
    'ui/widgets/inline_edit.cpp',
    'ui/widgets/mode_button.cpp',
    'ui/widgets/panel.cpp',
    'ui/widgets/segment.cpp',
    'ui/widgets/split_pane.cpp',
)
if get_option('enable_debugger')
    sdlfltk_cpp_sources += files(
        'ui/console/console_format.cpp',
        'ui/console/console_text.cpp',
        'ui/console/console_view.cpp',
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
        'ui/debugger/project_state.cpp',
        'ui/debugger/registers_view.cpp',
        'ui/debugger/stack_view.cpp',
        'ui/debugger/state_pill.cpp',
        'ui/debugger/toolbar.cpp',
        'ui/debugger/watch_list.cpp',
        'ui/debugger/watch_model.cpp',
        'ui/debugger/watch_panel.cpp',
        'ui/debugger/watchpoint_actions.cpp',
        'ui/debugger/watchpoint_list.cpp',
        'ui/hardware/hw_register_view.cpp',
        'ui/hardware/input_view.cpp',
        'ui/hardware/io_registers_view.cpp',
        'ui/hardware/sound_registers_view.cpp',
        'ui/hardware/video_registers_view.cpp',
        'ui/memtools/candidate_list.cpp',
        'ui/memtools/change_search.cpp',
        'ui/memtools/change_search_window.cpp',
        'ui/memtools/form_dialog.cpp',
        'ui/memtools/memory_actions.cpp',
        'ui/memtools/pattern.cpp',
        'ui/profiler/hot_code_view.cpp',
        'ui/profiler/hot_list.cpp',
        'ui/profiler/semihost_view.cpp',
        'ui/symbols/map_files.cpp',
        'ui/symbols/symbol_list.cpp',
        'ui/symbols/symbols_view.cpp',
        'ui/tools/tool_registry.cpp',
        'ui/tools/tool_tab.cpp',
        'ui/tools/tool_window.cpp',
        'ui/tools/tool_windows.cpp',
        'ui/video/inspector.cpp',
        'ui/video/sprite_data.cpp',
        'ui/video/sprite_inspector.cpp',
        'ui/video/sprite_sheet.cpp',
        'ui/video/video_window.cpp',
        'ui/video/video_windows.cpp',
        'ui/video/vram_image.cpp',
    )
else
    sdlfltk_cpp_sources += files('ui/app/debugger_ui_disabled.cpp')
endif
