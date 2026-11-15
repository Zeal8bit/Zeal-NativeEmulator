# Raylib dependencies

All dependencies use Raylib **6.2**, with matching `raylib.h`, `raymath.h`, and
`rlgl.h` headers from the same release. Upstream files are unmodified.

| Directory | Target | Library source |
| --- | --- | --- |
| `linux64/` | Linux x86-64 | Official Linux amd64 release, shared library |
| `win64/` | Windows x86-64, MinGW-w64 | Official Win64 MinGW-w64 release, static library |
| `web/` | WebAssembly | Generated locally or in CI from release sources with Emscripten 7.0.0; see [build instructions](web/README.md) |

Generated web headers and libraries are ignored by Git. Run
`python3 tools/build_raylib_web.py` before configuring a fresh WASM build.

## Native release archives

Download from the [upstream 6.2 release](https://github.com/raysan5/raylib/releases/tag/6.2)
and verify the SHA-256 before replacing libraries and headers together:

| Archive | SHA-256 |
| --- | --- |
| `raylib-6.2_linux_amd64.tar.gz` | `586bbc32f3596d439382b1c5dbc6c1c573eb7e1d00fe92be7d4deb116883720d` |
| `raylib-6.2_win64_mingw-w64.zip` | `48ba378d100fe7e13fc2c35d708764b855b815596f3b0a31746e37259283961f` |

For Linux, retain `libraylib.so.6.2.0` and the upstream symlink chain:
`libraylib.so` → `libraylib.so.620` → `libraylib.so.6.2.0`.
The official Linux binary requires **glibc 2.38 or newer** (for example,
Ubuntu 24.04+ or Debian 13+). At runtime, make its `lib/` directory available
through `LD_LIBRARY_PATH`, or install the matching shared library.
For Windows, retain the static `libraylib.a`; the DLL/import library is not used.
Copy all three headers from each archive's `include/` directory and preserve
its license. The native bundles use [LICENSE.txt](LICENSE.txt).

Select a bundle with `-Draylib_path=/absolute/path/to/raylib/linux64` or
`-Draylib_path=/absolute/path/to/raylib/win64`. The compiler must target the
corresponding architecture. macOS uses the system-installed Raylib.
