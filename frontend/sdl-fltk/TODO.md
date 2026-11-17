# zeal-fltk TODO

Ideas and follow-ups for the SDL3 + FLTK frontend that are not done yet.
Roughly ordered by value within each section.

## Debugger: needs new core hooks

- **Memory watchpoints (break on read/write).** The core stores watchpoints
  (`debugger_add_watchpoint`) but never checks them; add the check in the MMU
  access path, then a UI (memory context menu "Break on write", list next to
  breakpoints). Classic "who is overwriting my variable" tool.
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

## UI polish

- Disassembly: a click selects a line and a double-click toggles a breakpoint;
  drop the double-click for consistency with the "click = primary action,
  right-click = menu" rule (gutter click and the context menu remain).
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
