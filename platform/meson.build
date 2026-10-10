# Platform backends. Each seam in include/platform/ has exactly one implementation
# linked into a given binary, chosen here. Every subdirectory below is one seam and
# holds only its backends.
#
# "auto" prefers SDL2 on desktop, because that is the backend that lets a build drop
# Raylib entirely, and falls back to Raylib when SDL2 is not available.
#
# The FLTK display backend is C++ and needs FLTK's compile flags, which only the UI
# library has. This directory still decides *that* it is the backend to use, but
# ui/fltk/meson.build adds the source to fltk_sources and fills platform_fltk_deps,
# so the UI layer never reaches into platform/ by path.
sdl_dep = dependency('sdl2', required: false)

# Dependencies the executable must compile and link against for the FLTK-selected
# backend. Filled in by ui/fltk/meson.build, which owns FLTK's compile flags.
platform_fltk_deps = []
platform_fltk_sources = []

if software_blitter and fltk_ui
    platform_fltk_sources += files('display/fltk.cpp')
else
    sources += files('display/raylib.c')
endif

audio_backend = get_option('audio_backend')
if audio_backend == 'auto'
    audio_backend = sdl_dep.found() ? 'sdl' : 'raylib'
endif
if audio_backend == 'sdl'
    if not sdl_dep.found()
        error('audio_backend=sdl requires SDL2 (pkg-config sdl2)')
    endif
    sources += files('audio/sdl.c')
elif audio_backend == 'raylib'
    sources += files('audio/raylib.c')
else
    sources += files('audio/null.c')
endif

controller_backend = get_option('controller_backend')
if controller_backend == 'auto'
    controller_backend = sdl_dep.found() ? 'sdl' : 'raylib'
endif
if controller_backend == 'sdl'
    if not sdl_dep.found()
        error('controller_backend=sdl requires SDL2 (pkg-config sdl2)')
    endif
    sources += files('controller/sdl.c')
elif controller_backend == 'raylib'
    sources += files('controller/raylib.c')
else
    sources += files('controller/null.c')
endif

# Backend dependencies the executable must compile and link against. The platform
# sources are compiled into the executable directly, so their include paths belong there.
platform_deps = (audio_backend == 'sdl' or controller_backend == 'sdl') ? [sdl_dep] : []
