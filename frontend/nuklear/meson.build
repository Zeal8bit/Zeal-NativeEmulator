nuklear_sources = files(
    'audio.c',
    'controller.c',
    'frontend.c',
    'mouse.c',
    'notif.c',
    'snes_adapter.c',
    'video.c',
    'window.c',
)
if get_option('enable_debugger')
    nuklear_sources += files(
    'debug_video.c',
    'debugger/debugger_ui.c',
    'debugger/panels/breakpoints.c',
    'debugger/panels/cpu.c',
    'debugger/panels/disassembler.c',
    'debugger/panels/display.c',
    'debugger/panels/memory.c',
    'debugger/panels/menubar.c',
    'debugger/panels/mmu.c',
    'debugger/panels/semihost.c',
    'debugger/panels/vram.c',
    'debugger/raylib-nuklear.c',
    'input.c',
)
endif
