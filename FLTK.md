# FLTK debugger with dockable panels and independent themes

## Summary

Replace desktop Nuklear debugger with FLTK interface. Raylib stays responsible for emulation rendering, audio, and controllers. Normal emulation window stays Raylib; debugger mode shows video inside dockable FLTK panel.

Support macOS, Linux, Windows. Remove Nuklear entirely, including its web frontend, bundled headers, integration code, and build dependencies. Web builds retain Raylib emulation and the C debugger backend; a separate web UI will be implemented later. Keep console/headless behavior.

Use FLTK 1.4.x, C++17 frontend, existing C core. FLTK supplies layout/widget primitives; app supplies docking and semantic theme layers. [FLTK layout documentation](https://www.fltk.org/doc-1.4/coordinates.html)

The emulator, hardware models, Raylib integration, debugger backend, and public APIs remain implemented and compiled in C. Only the FLTK UI, docking, and themes use C++. Desktop FLTK and the future web UI consume the same frontend-independent C APIs.

## Architecture and integration

- Place all FLTK C++ frontend code in new repository-root `ui/fltk/`: application shell, panels, docking, themes, shared widgets, and frontend-side C integration entry points. Give this directory its own Meson source/dependency definition; keep FLTK headers and dependencies inside this frontend boundary.
- Keep `hw/debugger/` as the C debugger backend and public API implementation directory: execution/debugger services, breakpoints/watchpoints, disassembly, inspection accessors, and binding adapters. Remove its existing Nuklear panel/UI sources as FLTK replaces them; do not put new FLTK code under `hw/`.
- Keep public C API declarations and plain data types in `include/debugger/`; backend-only implementation headers must not be part of the frontend-facing API. Existing machine-specific integration may remain in `hw/zeal_debugger.c`, called through the C backend rather than accessed directly by panels.
- Separate the C debugger backend and public API from the FLTK C++ frontend. Reuse existing debugger callbacks internally; remove Nuklear types from shared headers and all Nuklear-specific runtime paths.
- Expose frontend lifecycle, refresh, input, configuration, and visibility through a separate C-compatible integration interface. Debugger capabilities must work without creating a frontend or attaching a window; FLTK panels must not reach directly into machine internals.
- Add Meson `debugger_ui=auto|fltk|none`. With `enable_debugger=true`, final `auto` selects FLTK on desktop and no built-in debugger UI on web. `none` retains the C backend for console, API, and future web consumers. With `enable_debugger=false`, omit the backend and UI; reject an explicit incompatible `fltk` selection. Reject FLTK selection for web builds. Require C++/FLTK only when the FLTK frontend is selected.
- Compile `.c` sources as C and FLTK `.cpp` sources as C++17. Use the C++ linker only for FLTK executables. Web and UI-free builds must not require FLTK or a C++ runtime.
- Keep machine execution, Raylib graphics operations, and FLTK callbacks on main thread. Introduce bounded execution slices so UI events run even when guest produces no frames. Avoid nested event loops and callbacks that resume emulation recursively.
- In FLTK debugger mode, hide Raylib window while retaining live graphics context. Continue Raylib event/controller processing; prevent hidden-window throttling. Restore original window, focus, and pacing when debugger disabled.
- Transfer completed Raylib video into reusable RGBA buffers; FLTK draws pixels without sharing graphics contexts. Frame descriptor includes dimensions, stride, and generation; frontend copies borrowed data before producer reuses it. Normalize orientation and preserve nearest-neighbor scaling.
- Copy existing CPU-side VRAM images for visible inspector tab. Update video at guest frame cadence, inspectors at capped refresh rate, and immediately after pause/step/edit.
- Introduce shared host-input adapter. FLTK supplies focused Video keyboard/mouse events; Raylib supplies normal-mode events and gamepads. Preserve shortcuts, passthrough, key repeat, SNES mouse capture/speed, and coordinate scaling. Release held input on focus loss, hiding, detaching, and mode changes.

## Shared C debugger API

- Extend and clean up the existing C debugger API instead of implementing debugging logic in UI callbacks. Public headers expose opaque context handles, plain C data types, and functions with `extern "C"` guards for C++ consumers. Move backend implementation structs and callback wiring out of public headers. No FLTK, Nuklear, C++ classes, or Raylib graphics types in public debugger contracts.
- Expose execution state, pause, continue, step, step-over, reset, and orderly stop. Pause preserves a resumable machine; stop requests termination of the current emulation run and returns control to the host for cleanup. Neither command calls process exit or owns UI windows. Debugger activation must be separate from frontend visibility and available in web/headless operation.
- Expose breakpoint and watchpoint management/enumeration, register snapshots and edits, memory reads/writes, disassembly, and symbol lookup/loading. Document virtual versus physical address semantics explicitly. Provide symbol loading from caller-supplied text/data as well as the existing native file path flow so browser consumers are not forced to access host files.
- Add typed C accessors for MMU mappings, Semihost statistics and break-on-update controls, VRAM metadata/images, and video frames. Cover all panel data and actions through the API rather than casting debugger context pointers to machine structs. Keep rendered-image access separate from core inspection so headless debugging does not require a graphics context.
- Define consistent status/error results, buffer capacities and actual counts, and ownership/lifetime rules. Use fixed-width fields for addresses, register values, counters, and event data; copy inspection data into caller-owned buffers. Never expose mutable pointers to emulator internals. Existing borrowed frame views remain explicitly lifetime-bounded and have a copy path for binding consumers.
- Execute API operations on the emulation-owning thread between bounded execution slices. Reads return coherent snapshots; writes and commands take effect at a documented execution boundary. Pause remains responsive without guest video output. Do not promise concurrent calls or invoke frontend callbacks during partially updated machine state.
- Expose execution/stop reason and sequenced event records for state changes, breakpoint/watchpoint hits, and step completion. Provide bounded event retrieval with explicit overflow reporting; current state stays queryable independently of event delivery. FLTK consumes this same interface rather than special backend hooks.
- Keep API declarations and implementation usable in native C, C++, and WASM builds. Add a small explicitly exported C binding surface for browser access to handles, commands, and caller-provided buffers, without Embind or C++ dependencies. Document buffer allocation/release and fixed-width field marshalling; do not serialize raw native pointers or assume native and WASM struct layouts match. Browser presentation and higher-level JavaScript UI remain deferred.

## Workspace and feature parity

- Build separate docking manager using nested split nodes, tab groups, and floating FLTK windows. Support drag previews, edge splitting, tab stacking/reordering, detaching, redocking, and minimum sizes.
- Each panel has stable ID and independent state. Moving panels preserves edits, selection, scroll position, and subscriptions. Closing panel hides it; View menu restores it.
- Default arrangement: Video upper-left; CPU and Breakpoints beside it; Disassembler right; Memory below Video; MMU and Semihost below inspectors. VRAM starts hidden.
- Preserve all eight panels and existing interactions:

  - **Video:** live output, scale controls, paused indication, guest input.
  - **CPU:** registers, existing byte editing, flags, memory navigation, execution controls.
  - **Disassembler/Breakpoints:** symbols, current instruction, breakpoint markers, add/remove/toggle.
  - **Memory:** address/symbol navigation, range selection, hexadecimal case, ASCII/CP437, linked hover highlighting.
  - **MMU/Semihost:** mappings, device names, counter statistics, running state, break-on-update.
  - **VRAM:** both tilemap layers, tileset, palette, font, scrolling, hover previews and indices.

- Preserve menus, shortcuts, SNES configuration, volume controls, notifications, Save Config, and Reset Layout. Derive disassembler PC directly from debugger state so hiding CPU panel cannot freeze it.
- Closing main debugger window exits application, matching current window-close behavior. “Debugger Off” returns to normal emulation.
- Save versioned docking tree, tabs, visibility, split ratios, and floating geometry in separate flat INI workspace file under existing configuration directory. Validate tree and clamp restored windows to available displays. Invalid workspace restores defaults.
- First FLTK launch imports legacy panel visibility and debugger window size where available, then uses default docking arrangement. Legacy Nuklear geometry needs no ongoing compatibility; remove obsolete writers and ignore unused old keys.

## Independent themes

- Create `ThemeManager`, semantic style tokens, shared widget styling, and shared font service. Panels describe roles—selection, link, breakpoint, current instruction, paused, warning—without hardcoded appearance.
- Bundle Dark and Light presets; Dark default. Use restrained FLTK `gtk+` scheme with centralized palette, box treatment, spacing, and typography. FLTK schemes provide base widget appearance. [FLTK scheme documentation](https://www.fltk.org/doc-1.4/classFl.html)
- Support versioned flat INI theme files under configuration directory’s `themes` folder. Files choose bundled base preset and override colors, UI/monospace fonts, font sizes, spacing, and row heights. No scripts or recursive inheritance.
- Add Theme submenu: select preset/custom theme, reload selected file. Persist selection. Switching restyles all windows, invalidates text measurements, and relayouts without resetting panel state.
- Missing tokens inherit preset values. Malformed files leave current theme active and show actionable error; unavailable startup theme falls back to Dark.
- Preserve CP437 coverage through shared glyph rendering using bundled BigBlue font. Theme styling affects debugger chrome and overlays; emulated video and palette colors remain faithful.
- Theme editor and automatic OS appearance tracking deferred.

## Delivery and validation

1. **C API foundation:** establish `hw/debugger/` as the C backend/API implementation and `include/debugger/` as its public contract. Separate backend compilation from UI selection and expose missing debugger capabilities. Validate with UI-free C tests and a minimal WASM binding smoke test before panels depend on the API.
2. **Integration gate:** create `ui/fltk/` with minimal FLTK shell, live Video, CPU controls, and input bridge behind explicit build option, using the shared C API. Prove hidden Raylib rendering, event coexistence, pause responsiveness, audio, and mode transitions on all three desktop platforms before broad panel migration.
3. **Workspace/theme foundation:** implement docking, persistence, shared widgets, and live theme changes; then port remaining panels. Extend the C API whenever panel functionality needs additional emulator access.
4. **Parity and release:** update desktop dependency packaging and CI, including Windows cross-build support. Switch desktop default after parity checks pass. Delete Nuklear frontend sources, vendored UI headers, glue code, build options, and obsolete documentation/license references, retaining required third-party notices for any code still used. Remove browser controls that invoke the old debugger UI; web retains emulation and the reusable debugger API without a replacement debugger interface in this migration.

Validation covers:

- Unit tests for docking-tree operations/restoration, theme parsing/fallback, and input translation.
- C API tests for pause/continue/stop/reset, stepping, breakpoint/watchpoint hits, register edits, memory reads/writes, disassembly, symbols, MMU, and Semihost. Cover invalid handles/ranges, insufficient buffers, address-space semantics, snapshot consistency, event sequencing/overflow, and documented ownership.
- Compile public headers from both C and C++; verify frontend code does not include backend implementation structs. Exercise the API in a UI-free native C harness and through WASM exports, including command execution, memory buffer exchange, and state/event retrieval.
- Live checks for every panel interaction, repeated dock/undock, multi-monitor restoration, DPI changes, and themes with larger fonts.
- Shader and software blitters; text, bitmap, disabled-video modes; all VRAM tabs and pixel orientation.
- Editing while paused, breakpoint/step behavior, focus loss, passthrough, SNES mouse capture, and controller reconnects.
- Comparable ROM workloads against current frontend: sustain existing 60 FPS target where baseline achieves it, responsive controls, no new audio underruns. Record frame-copy overhead.
- Desktop FLTK builds, backend-enabled/UI-free builds, debugger-disabled builds, console/headless runs, and web builds with backend enabled/disabled. Verify no Nuklear remains and web/UI-free configurations require neither FLTK nor C++.

## Implementation status — feature/fltk-demo

Implemented the C backend/public API split, FLTK C++ frontend, eight panels, dock/tab/float workspace, versioned persistence, Dark/Light/custom themes, shared CP437 rendering, host input bridge, and explicit WASM bindings. Nuklear sources, bundled headers, and browser debugger controls are removed. Raylib still owns rendering, audio, and controller services. See [frontend guide](ui/fltk/README.md) for build commands, controls, theme format, API ownership, and browser buffer layouts.

Desktop `auto` selects FLTK on this demo branch. This is an implementation preview; the three-platform release gate above has not passed. Floating windows currently hold individual panels; nested splits and tab groups live in the main workspace.

Validated locally on macOS ARM with FLTK 1.4.5 and Raylib 5.5:

- Native FLTK builds with shader and software blitters; C API and workspace/theme/input model tests pass.
- UI-free native C build and real-machine tests pass, including CPU stepping, breakpoints/watchpoints, alternate registers, virtual/physical memory, MMU, Semihost, headless VRAM metadata, and stop.
- Debugger-disabled native and WASM builds pass. Backend-enabled WASM build and JavaScript exported-API smoke test pass.
- Compile commands confirm emulator sources compile as C; UI-free and WASM configurations contain no C++ sources. FLTK sources include no private hardware headers.
- Bounded rendered smoke tests run the bundled ROM, pause/step, detach/redock, switch Dark/Light/custom larger-font themes, inspect all five VRAM tabs, toggle debugger off/on, save configuration, capture screenshots, and terminate cleanly. Video orientation and CP437 rendering checked visually.
- Short smoke-run frame readback/copy measurements: shader average 850 µs, maximum 2055 µs across 140 copies; software average 847 µs, maximum 3534 µs across 138 copies. These are local smoke measurements, not sustained performance benchmarks.

Desktop CI recipes now build FLTK, run available native model tests, and stage dependencies/assets/licenses without an installation step. Windows uses target-built FLTK and matching C/C++ structure-layout flags. These CI and packaging changes have not been executed remotely.

Release validation still required:

- Linux and Windows live integration, packaged launch on clean machines, and macOS Intel execution.
- Multi-monitor restoration, DPI changes, exhaustive panel interactions, keyboard passthrough/focus transitions, SNES capture, and controller reconnects on each platform.
- Bitmap and disabled-video workloads, sustained 60 FPS comparison with the previous frontend, and audio-underrun measurements. Current rendered checks exercise the bundled text-mode ROM and VRAM inspectors.

The separate browser debugger presentation remains intentionally deferred; its C exports are available now.
