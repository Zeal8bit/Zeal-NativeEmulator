# Bundled web dependency

Raylib 6.2 (upstream release tag `6.2`, commit `0256645`), built from unmodified
upstream sources with Emscripten 7.0.0. Headers and library must be updated together.

Source: https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz

SHA-256: `df7a13129b1bec9e73a6962efff170beba310ee6eab8f94cd000b7316b4db5ee`

After activating Emscripten 7.0.0, run from the repository root:

```sh
python3 tools/build_raylib_web.py
```

The script verifies the source checksum and uses upstream's supported Makefile:
`PLATFORM=PLATFORM_WEB GRAPHICS=GRAPHICS_API_OPENGL_ES3 RAYLIB_BUILD_MODE=RELEASE`.
Use `--archive /path/to/6.2.tar.gz` to rebuild without downloading again.
WasmFS/Asyncify are selected when linking the emulator, not when compiling Raylib.
The native dependency bundles are independent. The web release license is in
[LICENSE.txt](LICENSE.txt).
