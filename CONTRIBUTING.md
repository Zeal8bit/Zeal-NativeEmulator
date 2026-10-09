# Development Guide

## Development Setup

Use GCC or Clang, Meson, Ninja, and Raylib 5.5 or newer. Install the platform
dependencies required by Raylib. Native builds need OpenGL support; browser
builds need WebGL2. See [README.md](README.md) for platform and runtime details.

Run commands from the repository root. For software-renderer development:

```sh
meson setup build-dev -Dblitter_software=true -Denable_debugger=true
meson compile -C build-dev
./build-dev/zeal-native --help
```

If Raylib is outside the compiler's search paths, add
`-Draylib_path=/path/to/raylib` when configuring. That directory must contain
`include/` and `lib/`. Use `meson configure build-dev` to inspect build settings;
[meson_options.txt](meson_options.txt) lists project options.

Provide a Zeal ROM through `--rom /path/to/rom.img` or the image locations
documented in the README. Keep local images and generated files untracked.

For WebAssembly, activate an Emscripten environment with `emcc`, `em++`, and
`emar` available, then configure a separate build:

```sh
meson setup build-wasm --cross-file wasm-cross.build -Dblitter_software=true
meson compile -C build-wasm
```

The WASM cross file disables the debugger by default; add
`-Denable_debugger=true` when needed. Serve the generated browser files through
an HTTP server rather than opening them directly.

## Project Structure

| Path | Purpose |
| --- | --- |
| `hw/main.c`, `hw/zeal.c` | Application entry point and machine integration. |
| `hw/` | CPU, memory, buses, storage, and other emulated devices. |
| `hw/zvb/` | Video-board components; `blitter/` contains renderer implementations. |
| `hw/debugger/`, `hw/zeal_debugger.c` | Debugger backend, UI panels, and machine integration. |
| `hw/userport/` | User-port device implementations. |
| `app/console/` | Headless console and debugger command handling. |
| `include/` | Headers organized by subsystem, plus bundled header libraries. |
| `utils/` | Configuration, paths, logging support, notifications, FIFO, and virtual timers. |
| `assets/` | Fonts, shaders, resources, and browser templates/support code. |
| `raylib/` | Bundled platform libraries, headers, and license. |
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
- Put function opening braces on their own line. Put control-flow opening braces
  on the same line as the condition, with a space after `if`, `for`, `while`, and
  `switch`. Write `} else {` and `} else if (...) {` on one line.
- Always use braces for control-flow bodies, including a single statement.
  Expand the body onto separate lines; do not write inline `if` statements.
- Declare one variable or struct member per line. Write one assignment per line;
  do not chain assignments. Split multiple statements onto separate lines.
- Write one `case` label per line, with its statements on following lines.
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
- Keep implementation-only functions and state `static`. Use `static inline`
  for small hot-path helpers where appropriate. Follow the existing device
  callback and initialization patterns instead of duplicating device plumbing.
- Use `#pragma once` in project headers. Include system/library headers before
  project headers; use quotes for project includes and include paths relative
  to `include/`.
- Preserve existing SPDX copyright and license headers. Add an appropriate SPDX
  header to new project source files. Comments should explain behavior, intent,
  or constraints; avoid references to private implementation sources.
- Use the helpers in `include/utils/log.h` for component diagnostics. Preserve
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

The repository currently has no configured code formatter or linter: there is no
`.clang-format`, `.clang-tidy`, `.editorconfig`, or formatting check in CI. Apply
these rules manually and inspect `git diff --check`. Do not run a formatter over
unrelated project code or bundled dependencies.

## Testing

Build the affected configuration and inspect compiler warnings. For changes
that interact with optional code, also build the relevant feature combinations
in separate directories, such as debugger enabled/disabled or software
scanline rendering enabled/disabled. The software renderer is the development
focus; the shader renderer is deprecated. Shared changes must still compile for
both renderer selections.

Use a known ROM to check the affected behavior in the debugger or headless
console. Headless commands advance a specified number of emulated CPU T-states:

```sh
printf 'run 10000\nquit\n' | ./build-dev/zeal-native --console --rom /path/to/rom.img
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
meson setup build-sanitize -Dblitter_software=true -Db_sanitize=address,undefined
meson compile -C build-sanitize
```

The existing platform CI workflows compile builds; they do not replace focused
behavior checks. Record the build configurations, workloads, and local checks
used to validate a change, along with any unavailable target validation.
