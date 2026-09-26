# FLTK debugger

The emulator and debugger backend remain C. This directory contains the C++17 frontend only. Panels use `include/debugger/api.h`; frontend lifecycle and host services use `include/debugger/frontend.h`. Private machine structures stay behind the C boundary.

## Build and run

Install Raylib and FLTK 1.4.x through your platform's package manager, or build FLTK inside the checkout using `tools/build_fltk.sh`. The helper builds libraries without installing them. FLTK 1.4 supports [configure/make builds](https://www.fltk.org/doc-1.4/intro.html).

```sh
meson setup build-fltk -Ddebugger_ui=fltk
meson compile -C build-fltk
./build-fltk/zeal-native --debug --rom roms/default.img
```

Use `-Draylib_path=/path/to/raylib` when necessary. For a locally built or cross-compiled FLTK, set `-Dfltk_config=/absolute/path/to/target/fltk-config`. Cross builds never silently use the host's `fltk-config`. Windows C and C++ compiler options must agree on structure layout.

`debugger_ui=auto` selects FLTK on desktop and no frontend on WASM. `debugger_ui=none` retains the C backend and console. `enable_debugger=false` excludes the backend and frontend. UI-free builds do not enable a C++ compiler or link a C++ runtime.

## Workspace

Drag a panel tab onto another panel's center to stack tabs, or onto an edge to split. Drop outside the main workspace to detach. Floating windows have a drag strip for redocking; double-click the strip to return to the workspace. Drag split boundaries to resize. Drop onto a tab header to reorder. Closing a panel hides it; View restores it. Closing the main window stops the emulator. File → Debugger Off returns to the Raylib window.

Each panel retains its widgets and inspection state across moves. File → Save Config saves the workspace and theme; clean exit also saves them. View → Reset Layout restores the default arrangement. Corrupt workspace data falls back to defaults; floating geometry is clamped to the available monitor. The first run imports legacy panel visibility and debugger dimensions.

Configuration lives in the usual `.zeal8bit` directory, or `ZEAL_CONFIG_DIR` when set. `fltk-workspace.ini` stores a versioned split tree, tab order/selection, hidden panels, and floating geometry. `fltk-preferences.ini` stores the theme and window dimensions. Existing emulator settings remain in `zeal.ini`.

## Panels and controls

- **Video:** live Raylib output with nearest-neighbor scaling. Click to focus guest keyboard input. Middle-click toggles mouse capture; wheel adjusts SNES mouse speed. Focus loss releases held input. Video menu changes zoom.
- **CPU:** edit register pairs in hexadecimal and press Enter while paused. Each pair exposes both bytes through its four hex digits. Right-click a field or double-click its label to navigate Memory. Flags appear below the registers.
- **Disassembler:** address/symbol entry, current-PC highlighting, and breakpoint markers. Click a row to toggle its breakpoint; right-click to open Memory. PC tracking works with the CPU panel hidden.
- **Breakpoints:** enter an address/symbol; choose breakpoint or read/write/read-write watchpoint. Click an existing entry to remove it. Watchpoints include instruction fetches and stop after the current instruction completes.
- **Memory:** address/symbol entry, selectable range, wheel scrolling, uppercase/lowercase hex, ASCII/CP437, and paired byte/character hover highlighting. CP437 uses the bundled BigBlue font rasterized through the C host.
- **MMU:** virtual/physical mappings and device names; click a mapping to navigate Memory.
- **Semihost:** eight counters with running state, latest/minimum/maximum/average time and sample counts. Click a counter to toggle break-on-update. Scroll to lower counters.
- **VRAM:** Layer 0, Layer 1, Tileset, Palette, and Font. Wheel scrolls vertically; Shift+wheel horizontally. Hover shows cell indices, values/attributes, color information, and a magnified preview.

CPU shortcuts use Command on macOS and Control elsewhere: F5 Continue, F6 Pause, F9 Toggle Breakpoint, F10 Step Over, F11 Step, Shift+Backspace Reset. Modifier+F1 toggles debugger mode. Keyboard Passthrough sends these keys to the guest when Video has focus. SNES port assignment, mouse speed reset, volume, and notifications are available through the shell.

## Themes

Theme code is independent of panel rendering. Panels request semantic roles such as `text`, `selection`, `breakpoint`, and `current`; widgets receive shared styling and font metrics. Theme changes preserve panel state.

Put custom `.ini` files in `ZEAL_CONFIG_DIR/themes` (or the normal config directory's `themes`). Files are discovered when the frontend starts; Theme → Reload reloads the selected file. Example:

```ini
version=1
base=Dark
background=#202730
surface=#171D25
text=#DCE4ED
link=#73B9EF
selection=#314E6B
ui_font=Helvetica
mono_font=Courier
font_size=13
mono_size=13
spacing=6
row_height=24
```

Other color roles: `muted`, `border`, `breakpoint`, `current`, `paused`, `warning`. Missing values inherit Dark or Light. Font names resolve through FLTK's font catalog, falling back to Helvetica/Courier. Row height must accommodate both fonts plus four pixels. Unknown keys, duplicate fields, malformed colors, and invalid sizes reject the entire file and preserve the active theme. An unavailable startup theme uses Dark. Guest pixels and palette values are never recolored.

## C API and browser bindings

`api.h` documents statuses, capacities, ownership, address spaces, and threading. The host owns `dbg_t`; consumers never allocate its private structure. Call APIs on the owning emulation thread between execution slices. UI callbacks issue commands without entering another emulation loop. Pause is resumable; Stop ends the current run and returns to host cleanup. Stop never calls process exit. Register/memory changes apply synchronously at this boundary.

All addresses for breakpoints, watchpoints, and disassembly are virtual 16-bit addresses. The checked memory API explicitly selects virtual (64 KiB) or physical (4 MiB) space and rejects wrapping ranges. Device writes retain hardware semantics, including ROM protection. Legacy disassembly reads wrap at the end of virtual memory so instructions crossing `FFFF` can be decoded. Rendered images require a graphics context; register/memory/MMU/Semihost/VRAM metadata inspection does not.

Snapshots and image data copy into caller-owned buffers. `debugger_get_symbol` returns an immutable string valid until debugger teardown. Event records have increasing 64-bit sequences; retrieve records after the last observed sequence. A 64-record ring reports overflow explicitly. State can always be queried independently. Image query/copy reports required dimensions and capacity; callers should retry if dimensions change. `debugger_render_stats` reports measured readback/copy costs.

WASM builds with `enable_debugger=true` export the functions in `bindings.h`. Obtain the current integer handle with `_zeal_debug_handle()` after emulator initialization. Handles become invalid on teardown; they are not machine pointers. Command values and status values match `api.h`. Allocate buffers using module `_malloc`, release with `_free`, and recreate JavaScript typed-array/DataView views after any call that may grow WASM memory. Keep pointers and capacities inside the allocated region. A stopped machine must be reinitialized by its host before resuming.

Bindings encode all integers explicitly in little-endian byte order; do not map native structs directly:

| Function | Buffer layout |
| --- | --- |
| `zeal_debug_state` | 24 bytes: u32 paused, stopped, reason, PC; u64 sequence |
| `zeal_debug_events` | 8-byte header: u32 count, overflow; then 20-byte records: u64 sequence, u32 reason, address, access. With only header capacity, count reports available records. |
| `zeal_debug_registers` | 26 bytes: u16 PC, SP, AF, BC, DE, HL, AF′, BC′, DE′, HL′, IX, IY, IR |
| `zeal_debug_points` | u32 total count; then pairs of u32 address, access (zero for breakpoints) |
| `zeal_debug_disassemble` | 136 bytes: u32 instruction size, 4 opcode bytes, 64-byte label, 64-byte instruction; strings NUL-terminated |
| `zeal_debug_mappings` | Four 76-byte entries: u32 virtual address, physical address, page; 64-byte device name |
| `zeal_debug_counters` | Eight 60-byte entries: six u64 values (start cycles, last split cycles, min µs, max µs, total interval µs, last µs), then u32 samples, running, break-on-update |
| `zeal_debug_vram` | 44 bytes: u32 mode, graphics, bitmap, cell width, cell height, columns, rows, index, value, attributes, palette RGB |
| `zeal_debug_image` | 20-byte descriptor: u32 width, height, stride; u64 generation. Separate RGBA8 pixel buffer, top-down rows. View -1 selects video; 0–4 select VRAM. |

`zeal_debug_memory` transfers plain bytes; its write and physical flags are 0/1. Symbol loading accepts caller-provided text (`name = $1234 ; addr, ...`). Browser presentation and higher-level JavaScript APIs remain separate future work.

## Validation

```sh
meson test -C build-fltk --print-errorlogs
meson setup build-api -Ddebugger_ui=none
meson test -C build-api --print-errorlogs
```

The C machine harness exercises real CPU stepping, breakpoint/watchpoint hits, alternate registers, physical/virtual memory, MMU, Semihost, stop, and headless image rejection. Model tests cover docking persistence, corrupt restoration, theme validation, and key translation. The WASM test runs the C tests and then exercises the exported ABI from JavaScript.

For a bounded desktop smoke test, configure with `-Dfltk_test_hooks=true`, compile, then run `tools/test_fltk.py build-fltk`. It opens all seven menus and checks popup bounds while moving the debugger across available monitors; macOS additionally verifies native window bounds. It then runs a ROM, pauses/steps, detaches/redocks, visits all VRAM tabs, changes fonts/themes, toggles debugger mode, captures screenshots, and exits. Test hooks are absent from normal builds. All test config and screenshots stay in the selected build directory. Repeat with `-Dblitter_software=true` for the software renderer.

See `FLTK.md` for implementation and platform validation status. CI builds are not a substitute for live multi-monitor, controller reconnect, audio, and DPI validation on each desktop platform.
