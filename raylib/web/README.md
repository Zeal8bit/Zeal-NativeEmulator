# Web dependency

Raylib 6.2 (upstream release tag `6.2`, commit `0256645`), built from unmodified
upstream sources with Emscripten 7.0.0. Generated headers and libraries are not
checked into the repository; the helper builds them together in this directory.
Git ignores generated files here while keeping this README and the license tracked.

Source: https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz

SHA-256: `df7a13129b1bec9e73a6962efff170beba310ee6eab8f94cd000b7316b4db5ee`

With Emscripten 7.0.0 on PATH (from the SDK or Homebrew), run from the repository
root before configuring the emulator's web build:

```sh
python3 tools/build_raylib_web.py
```

The script verifies the source checksum and uses upstream's supported Makefile:
`PLATFORM=PLATFORM_WEB GRAPHICS=GRAPHICS_API_OPENGL_ES3 RAYLIB_BUILD_MODE=RELEASE`.
Use `--archive /path/to/6.2.tar.gz` to rebuild without downloading again.
The WASM cross file uses `raylib/web/` by default. CI runs the same helper
before Meson setup. Re-run it when the pinned Raylib or Emscripten version changes.
WasmFS/Asyncify are selected when linking the emulator, not when compiling Raylib.
The web release license is in
[LICENSE.txt](LICENSE.txt).
