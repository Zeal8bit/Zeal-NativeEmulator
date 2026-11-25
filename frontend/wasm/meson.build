# Emscripten integration and browser output belong to this frontend.
wasm_sources = files('main.c', 'frontend.c', 'hostfs.c', 'audio.c',
    'controller.c', 'mouse.c', 'notif.c', 'snes_adapter.c', 'video.c')
wasm_sources += common_audio_sources
if debugger_api
    wasm_sources += files('web_debug.c')
endif
wasm_args = ['-D_DEFAULT_SOURCE', '--use-port=sdl3']
if get_option('show_fps')
    wasm_args += '-DCONFIG_SHOW_FPS=1'
endif
wasm_link_args = ['--emit-symbol-map',
             '--use-port=sdl3',
             '-sFORCE_FILESYSTEM=1',
             '-sWASMFS=1',
             '-sEXPORT_NAME="NativeModule"',
             '-sASYNCIFY=1',
             '-sMINIFY_HTML=1',
             '-sUSE_WEBGL2=1',
             '-sEXIT_RUNTIME=1',
             '-sMODULARIZE=1',
             '-sEXPORTED_RUNTIME_METHODS=["FS","PATH","HEAPF32","HEAPU8","getValue","setValue"]',
             '-sEXPORTED_FUNCTIONS=["_main","_malloc","_free","_zeal_exit","_zeal_exit_web","_zeal_flush_storage_web","_zeal_debug_toggle_web","_zeal_snes_button_web","_zeal_snes_clear_web","_hostfs_web_complete","_hostfs_web_write_guest"]',
             '-sASYNCIFY_STACK_SIZE=65536']
# Optional overrides supplied by a custom toolchain file.
wasm_link_args += meson.get_external_property('link_args', [])
