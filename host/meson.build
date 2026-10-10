# Host backends. Each seam in include/host/ has exactly one implementation linked into
# a given binary, chosen here.
#
# "auto" prefers SDL2 on desktop, because that is the backend that lets a build drop
# Raylib entirely, and falls back to Raylib when SDL2 is not available.
#
# The FLTK host backend is C++ and needs FLTK's compile flags, which only the UI library
# has, so it is added to fltk_sources by ui/fltk/meson.build rather than here.
sdl_dep = dependency('sdl2', required: false)

if software_blitter and fltk_ui
    # Desktop: the FLTK host is built with the UI library.
else
    sources += files('zeal_host_raylib.c')
endif

audio_backend = get_option('audio_backend')
if audio_backend == 'auto'
    audio_backend = sdl_dep.found() ? 'sdl' : 'raylib'
endif
if audio_backend == 'sdl'
    if not sdl_dep.found()
        error('audio_backend=sdl requires SDL2 (pkg-config sdl2)')
    endif
    sources += files('zeal_audio_sdl.c')
elif audio_backend == 'raylib'
    sources += files('zeal_audio_raylib.c')
else
    sources += files('zeal_audio_null.c')
endif

controller_backend = get_option('controller_backend')
if controller_backend == 'auto'
    controller_backend = sdl_dep.found() ? 'sdl' : 'raylib'
endif
if controller_backend == 'sdl'
    if not sdl_dep.found()
        error('controller_backend=sdl requires SDL2 (pkg-config sdl2)')
    endif
    sources += files('zeal_controller_sdl.c')
elif controller_backend == 'raylib'
    sources += files('zeal_controller_raylib.c')
else
    sources += files('zeal_controller_null.c')
endif

# Backend dependencies the executable must compile and link against. The host sources
# are compiled into the executable directly, so their include paths belong there.
host_deps = (audio_backend == 'sdl' or controller_backend == 'sdl') ? [sdl_dep] : []
