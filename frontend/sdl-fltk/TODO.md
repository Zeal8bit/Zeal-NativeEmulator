# zeal-fltk TODO

Ideas and follow-ups for the SDL3 + FLTK frontend. Roughly ordered by value
within each section; finished items are marked **Done** and kept for
reference.

## Debugger: needs new core hooks

- **Done: Memory watchpoints (break on read/write).** The CPU's data reads
  and writes are checked against a 64 KB map built from the watchpoints (one
  pointer test per access when none is set); the machine pauses after the
  instruction, and the address, value and PC are shown (notification, toolbar
  pill, status bar). Single bytes or ranges, $0000 included, 16-bit accesses
  caught per byte, debugger reads and opcode fetches ignored. UI: memory view
  context menu (Break on Read / Write / Access), a Watchpoints list under the
  breakpoints, `r`/`w`/`rw ADDR` in the breakpoint field, underlined bytes.
  Left to do:
  - DMA and HostFS transfers bypass the CPU and are not watched; physical
    (22-bit) addresses are not supported.
  - Only the first hit of an instruction is kept; a hit from an interrupt's
    stack push taken in the same step is attributed to the previous PC.
  - Watchpoints (and breakpoints) are not saved in the configuration.
- **Trace log / execution history.** Ring buffer of the last N executed
  instructions with registers ("how did I get here"). Enables a heuristic
  call stack by tracking CALL/RET/RST and interrupts.
- **Code/data coverage and PC heatmap.** Shade executed and read/written bytes
  in the disassembly and memory views (like Mesen's CDL); spot dead code and
  hot loops.
- **Event viewer.** Plot writes to video/sound registers by scanline and cycle;
  essential for raster effects and mid-frame tricks.

## Core fixes noticed while building the frontend

- **PS/2 scancode table (`hw/keyboard.c`).** No scancodes for keypad Enter
  (`E0 5A`), Right Ctrl, Menu and keypad `=`; Left Ctrl is mapped to `E0 14`
  (Right Ctrl's code, Left Ctrl is `14`). Affects zeal-native too.
- **CLI breakpoints parse a leading `0` as octal** (`-b 0236` stops at `$009E`).
  `debugger_set_breakpoints_str` uses `strtoul(..., 0)`; addresses are
  conventionally hex.

## Follow-ups from the debugger panels

- **UART input in the Console.** Typing into the emulated UART needs core
  support: `read_rx` in `hw/uart.c` is a stub and input only comes from stdin
  (`host_read_byte`).
- **Watch:** write values from the panel; long expressions are clipped in the
  narrow column.
  - **Done:** the list is capped at 64 entries instead of 16. Each watch is one
    `FLTK_WATCH_<n>` config value and rini now holds 512 (about 100 used).
- **Hot Functions:** expose the sampling interval in the UI; optionally cap the
  "nearest preceding symbol" distance so sparse maps don't blame far-away code;
  samples are per 16-bit logical address (MMU-swapped code at the same address
  is merged).
- **Hardware views:** only the first 16 palette entries are shown; timer,
  scanline and hpos registers are only updated by the core when the guest
  touches them, so they read stale values while running.
- **Done: CPU timing.** The core counts frames (`zvb_t.frame_count`, reset
  with the video board) and the raster timing constants are public in
  `hw/zvb/zvb.h`; `bridge/cpu.c` no longer derives or copies them.
- **Not exercised by hand yet:** right-click menus, file dialogs (memory
  save/load, console save), clipboard and drag-selection were implemented by
  following existing patterns but could not be clicked during development.

## UI polish

- **Done: Disassembly clicks.** A click selects the line, a click on the left
  edge (gutter) toggles a breakpoint, right-click opens the menu; no
  double-click.
- Sprite viewer: dim off-screen sprites less and add an "OFF" tag so their art
  stays readable; legend under the screen map ("filled = inspected, outline =
  on screen"); dashed outline for sprites behind the foreground.
- Window/geometry: restore the Video & Profiler window position reliably on
  multi-monitor setups.

## Platform

- Windows build (MinGW/MSVC) with FLTK + SDL3, and release packaging of
  `zeal-fltk` (static SDL3 from `tools/build_sdl3.py`).
- Wayland: SNES mouse capture only hides the cursor (no pointer warp); use the
  pointer-constraints protocol when FLTK exposes it.
- Non-US host keyboard layouts: keys are mapped by character (US physical
  positions for shifted symbols); consider physical scancodes.
