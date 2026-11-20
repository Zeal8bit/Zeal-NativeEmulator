# Development Guide

## Development Setup

Use GCC or Clang, Meson, and Ninja. Desktop builds require a C++17 compiler, FLTK 1.4+, SDL3 and OpenGL. Browser builds use the Emscripten SDL3 port and WebGL2. Core-only/headless builds need no graphical dependencies. See [README.md](README.md) for platform and runtime details.

Run commands from the repository root. For development:

```sh
meson setup build-dev
meson compile -C build-dev
./build-dev/zeal-native --help
```

Use `meson configure build-dev` to inspect build settings;
[meson_options.txt](meson_options.txt) lists project options.

Provide a Zeal ROM through `--rom /path/to/rom.img` or the image locations
documented in the README. Keep local images and generated files untracked.

For WebAssembly, activate Emscripten **7.0.0** with `emcc`, `em++`, and
`emar` available, then configure a separate build:

```sh
meson setup build-wasm --cross-file frontend/wasm/wasm-cross.build
meson compile -C build-wasm
```

The WASM cross file disables the core debugger API by default; add
`-Denable_debugger=true` when needed. The WASM frontend never includes the
desktop debugger UI. Serve the generated browser files through
an HTTP server rather than opening them directly. Emscripten fetches its SDL3 port during the first build. See the [browser configuration guide](frontend/wasm/README.md).

## Project Structure

| Path | Purpose |
| --- | --- |
| `hw/` | Host-independent CPU, memory, buses, device protocols, and machine lifecycle. |
| `hw/zvb/` | Video-board components and RGB565 software frame generation. |
| `hw/debugger/`, `hw/zeal_debugger.c` | Debugger backend and machine integration. |
| `hw/userport/` | SNES serial protocol. |
| `frontend/common/` | Launch options, image files, HostFS, RTC and stream adapters. |
| `frontend/sdl-fltk/` | Desktop-only window loop, input, audio playback, and all desktop debugger UI. |
| `frontend/wasm/` | SDL3 browser frontend, templates, Emscripten exports, OPFS startup and browser HostFS. |
| `frontend/common/desktop/` | Desktop OS startup, paths and HostFS shared by SDL/FLTK and headless. |
| `frontend/headless/` | Batch execution and console/debugger commands. |
| `include/` | Headers organized by subsystem, plus bundled header libraries. |
| `utils/` | Portable FIFO and per-machine virtual timers. |
| `assets/` | Desktop fonts and resources. |
| `tools/` | Build-time resource and WASM helper scripts. |
| `meson.build`, subsystem `meson.build` files | Build configuration and source lists. |
| `.github/` | Platform build and deployment workflows. |
| `tests/`, `tmp/`, `build-*/` | Ignored local checks, scratch files, and build output. |

Use Meson for development builds. Place implementation in the owning subsystem
and declarations in its corresponding header directory. Update the relevant
Meson source list when adding production source files. Bundled third-party
sources retain their own conventions and licenses.

## Coding Conventions

Meson selects C99 for C sources; follow the compiler features already used by
the project rather than introducing a new language requirement.

These rules apply to new and changed project code. Older code contains style
variations; avoid reformatting unrelated lines.

- Use four spaces for indentation, no trailing whitespace, and a final newline.
- Use `snake_case` for functions and variables, subsystem prefixes for exposed
  names, and `_t` for typedef names. Use `UPPER_SNAKE_CASE` for macros.
- Choose argument and variable names that describe their role. Avoid single-letter
  names and opaque abbreviations; prefer names such as `bytes_written`,
  `page_offset`, and `source_directory`.
- Put function opening braces on their own line. Put control-flow opening braces
  on the same line as the condition, with a space after `if`, `for`, `while`, and
  `switch`. Write `} else {` and `} else if (...) {` on one line.
  Keep opening braces for struct, enum, and union definitions on the same line.
- Use braces for control-flow bodies. Short guard statements such as
  `if (!request) return;` and `if (machine->should_exit) break;` may omit braces
  and stay on one line. Expand braced bodies onto separate lines.
- Declare one variable or struct member per line. Write one assignment per line;
  do not chain assignments. Split multiple statements onto separate lines.
- Write one `case` label per line, with its statements on following lines.
  Indent case labels inside the `switch` and their statements one level further.
  Preserve intentional fallthrough and use a block when a case needs local
  declarations.
- Name register indexes, address boundaries, masks, dimensions, and timing
  constants with descriptive macros. Reuse existing definitions. Preserve
  literal widths, signedness, and expression grouping when replacing numbers.
- Use `<stdint.h>` types for register values, addresses, stored widths, and
  unsigned quantities. Use `int` where an ordinary counter or signed result is
  appropriate, `size_t` for host buffer sizes, and `bool` for boolean state.
  Avoid bare `unsigned` declarations.
- Match the prevailing pointer style, such as `const uint8_t* data`. Use `const`
  for inputs and local values that are not modified.
  Keep a space after C-style casts, as in `(flash_t*) device`. Preserve aligned
  declarations, assignments, macro values, and related array entries for readability.
- Keep implementation-only functions and state `static`. Use `static inline`
  for small hot-path helpers where appropriate. Follow the existing device
  callback and initialization patterns instead of duplicating device plumbing.
- Use `#pragma once` in project headers. Include system/library headers before
  project headers; use quotes for project includes and include paths relative
  to `include/`.
- Preserve existing SPDX copyright and license headers. Add an appropriate SPDX
  header to new project source files. Comments should explain behavior, intent,
  or constraints; avoid references to private implementation sources.
- Use instance-bound `hw_host_t` callbacks for core diagnostics and the helpers
  in `include/utils/log.h` for frontend diagnostics. Preserve
  useful debug logging and its existing verbosity controls.
- Keep platform and optional-feature code behind the existing build macros.
  Changes must remain buildable with the relevant feature enabled and disabled.

For example:

```c
static inline uint32_t next_address(uint32_t address, int operation)
{
    if (operation == OP_INCREMENT) {
        address++;
    } else if (operation == OP_DECREMENT) {
        address--;
    }
    return address & ADDRESS_MASK;
}
```

The repository `.clang-format` configures C/C++ formatting, including one-line
short guard statements and `} else {` on one line. Use file-based style in your
editor so format on save picks up these settings. There is no formatting check
in CI; inspect `git diff --check`. Do not run a formatter over unrelated project
code or bundled dependencies.

Use `clang-format off`/`clang-format on` around manually aligned tables when the
formatter cannot preserve their grouping and spacing.

## Testing

Build the affected configuration and inspect compiler warnings. For changes
that interact with optional code, also build the relevant feature combinations
in separate directories, such as WASM debugger enabled/disabled or software
scanline rendering enabled/disabled. Use `-Dscanline_rendering=false` to
validate builds without scanline support.

Use a known ROM to check the affected behavior in the debugger or headless
console. Headless commands advance a specified number of emulated CPU T-states:

```sh
printf 'run 10000\nquit\n' | ./build-dev/zeal-headless --console --rom /path/to/rom.img
```

Use scratch images when checking storage writes. For UI or rendering changes,
also inspect the visible output. For performance-sensitive changes, compare
the same workload, compiler settings, and build options before and after the
change; distinguish CPU emulation/rendering time from host presentation time.

There is no committed automated test suite or Meson test target. Local C
regression harnesses may live under the gitignored `tests/` directory, but are
not available in a fresh checkout. Compile and run the relevant harnesses
directly against the affected components. Keep them and generated binaries out
of commits, and do not add them to Meson.

Where supported, use AddressSanitizer and UndefinedBehaviorSanitizer for memory
and arithmetic checks. A separate instrumented application build can be made
with:

```sh
meson setup build-sanitize -Db_sanitize=address,undefined
meson compile -C build-sanitize
```

The existing platform CI workflows compile builds; they do not replace focused
behavior checks. Record the build configurations, workloads, and local checks
used to validate a change, along with any unavailable target validation.

## Boundary validation

Build `-Dfrontend=none` and `-Dfrontend=headless` without graphical paths, then
build both native frontends with scanline support disabled and profiling
enabled. `zeal_dep` exports the core headers, feature definitions, and portable
link dependencies. Do not add host macros, graphical dependencies, or frontend
includes to this dependency. Keep mutable core data in machine/device contexts;
shared lookup tables must be immutable.

Use local harnesses for CPU/device timing, independent contexts, frames/PCM,
console transcripts, INI serialization, and scratch-media persistence. Match
against the unchanged build before accepting an ownership refactor. Native
and browser appearance/input still need interactive verification in addition
to build and API checks. The root Makefile delegates native builds to Meson.

WASM-specific code and link flags belong in `frontend/wasm/`. Keep desktop UI
code in `frontend/sdl-fltk/`; neither frontend may include or compile the other.
There is no shared graphics layer. Core-debugger-enabled WASM builds must still
exclude FLTK, debugger panels, and debug textures. Preserve build-root web
artifact names when changing Meson: declaring the executable in a subdirectory
would change the existing browser URLs.
