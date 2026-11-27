---
name: zeal-headless
description: Build, debug, profile and regression-test Zeal 8-bit apps and games using the zeal-headless emulator, its stdin debugger and emulated Z80 timing.
---

# Zeal headless

Run `zeal-headless --version` and `--help` to identify the build and its launch
options. In a console session, send `help` for commands and supported key names.
To refresh this skill from the installed executable, run `zeal-headless --skill`.

## Build and launch the target

Use the app's documented toolchain and packaging command. Keep the executable,
runtime assets and linker map from the same successful build. If building the
emulator itself, use `meson setup build-headless -Dfrontend=headless` followed by
`meson compile -C build-headless`. The resulting binary is `build-headless/zeal-headless`.

Choose the launch path from the target's boot requirements. Supply a bootable
image with `--rom`. For an OS program, boot the required OS ROM, stage the
program and assets under `--hostfs DIR`, then launch it via the guest shell or
its startup script. To type a guest shell command through the console, send
keyboard events using the guest layout and allow time for the OS to poll.
For a Zeal OS ROM with a replaceable romdisk init program, use
`--uprog FILE[,HEXADDR]`. A reset reloads the injected program.

Run tests in a temporary working directory with a dedicated HostFS tree.
Use copies for explicit ROM, EEPROM (`--eeprom`), TF/SD (`--tf`) and CompactFlash
(`--cf`) images that the guest may write. Headless gets settings from arguments.
For omitted ROM, EEPROM and TF paths it also searches the corresponding
`ZEAL_NATIVE_ROM`, `ZEAL_NATIVE_EEPROM` and `ZEAL_NATIVE_TF` environment variables,
then `roms/` under the working directory and `~/.zeal8bit/`. Isolate those
locations when testing without persistent media.

## Drive a repeatable scenario

For a batch smoke run, use `zeal-headless --rom boot.img --headless 1000000`.
A positive budget bounds emulated T-states. For a scripted session, start
`zeal-headless --rom boot.img --console` (`--debug` is an alias). Send one
whitespace-delimited command per line. Blank lines and whole-line comments
starting with `#` are accepted. Commands run synchronously.

For example, with a bootable ROM and its matching map, replace `_update` with an
actual routine reached by the guest. Set a wall-clock timeout in the process
runner. The first register dump should show the routine's entry address.

```sh
zeal-headless --console --rom boot.img --map boot.map <<'COMMANDS'
bp _update
run 1000000
regs
mmu
u _update 4
bc _update
s
quit
COMMANDS
```

- Set or remove a breakpoint with `bp ADDR` or `bc ADDR`, list with `bl`, or
  pass `--brk ADDR_OR_SYMBOL[,ADDR_OR_SYMBOL...]` at launch. Use exact map
  symbols or `0x`-prefixed console addresses. Check the resolved PC and mapping.
- `run N` resumes for a positive T-state budget and may stop early. `c`
  (`continue`) waits for a debugger stop without a budget. A queued `p`
  (`pause`) cannot interrupt a command still executing. Use `s` (`step`) for
  one instruction and `so` (`step_over`) to step over a call.
- Inspect registers with `regs` and disassemble with `u [ADDR [COUNT]]`.
  Inspect mapped memory with `rb ADDR [COUNT]` (1–256 bytes), or read a
  little-endian word or double word with `r16 ADDR` or `r32 ADDR`. Use `mmu`
  at the checkpoint to identify the active bank. For a graphics assertion,
  inspect video memory while the relevant region is mapped.
- `wb ADDR BYTE ...` injects test state. Use up to six bytes per line.
  For an end-to-end input test, reach a verified ready state, `press RIGHT`,
  `run N` long enough for the guest to poll, `release RIGHT`, then advance
  again. `tap KEY` queues both events together. Choose keys and delays from
  the app's input loop.
- `reset` resets the emulator. `--no-reset` requests exit on reset.
  End scripts with `quit` or EOF. Capture stdout, stderr and exit status.
  Match expected PCs, memory values or guest files in order. A missing
  checkpoint or debugger error can occur even with a successful process exit.

## Automate assertions

For a Python harness, pass the absolute emulator path and stage the ROM and
assets in a disposable directory. Preserve the guest directory structure.
The example expects a staged `boot.img` and `host/` and reads the CPU-visible
reset vector before execution. Supply expected bytes from the build under test.

```python
import subprocess

def check_reset_vector(executable, workdir, expected):
    commands = "rb 0x0 3\nquit\n"
    result = subprocess.run(
        [executable, "--console", "--rom", "boot.img", "--hostfs", "host"],
        cwd=workdir, input=commands, text=True, capture_output=True, timeout=10,
    )
    dump = f"[DBG] 0000: {expected.hex(' ')}"
    if result.returncode or dump not in result.stdout:
        raise AssertionError((commands, result.stdout, result.stderr))
```

Apply the image-search isolation above before launching the harness. For larger
`rb` reads, verify the address and bytes of every output row. For interactive
drivers, drain stdout and stderr concurrently. Retain the command stream, logs
and relevant guest output files on failure before deleting the fixture.
Keyboard injection exercises the guest keyboard path. Controller-specific
behavior requires a separate input source.

## Measure guest work

Load the matching map, set a breakpoint at a verified operation boundary and
use `c` to reach it. The stop line includes `Paused @ 0x... (cyc=...)`. Clear
the entry breakpoint, set one at completion, continue and subtract the two
cycle counts. For steady-state update intervals, collect successive stops at
a boundary reached once per update. Check each PC and reject incomplete pairs.
Use a host timeout for every unattended continue or step-over.

Preserve the baseline binary, matching symbols and packaged assets before
rebuilding. Record toolchain, build flags, emulator and artifact hashes, CPU
frequency and video mode. Compare identical inputs, initial media, emulator
options and guest workload. Verify that each run reaches the intended state.
Report sample count and the distribution of elapsed T-states. Separate guest
work from intentional waits. Convert intervals using the configured emulated
CPU frequency and call them frame times only if each checkpoint corresponds
to a presented frame.

For a loop that waits for blanking to begin and end, identify both waits in
the linked code. Collect four ordered cycle stamps per update: start-wait
entry, start-wait return, end-wait entry and end-wait return. Clear temporary
entry breakpoints once hit so a polling iteration cannot become a new sample.
Work during blanking runs from the first return to the second entry. Work
outside blanking runs from the second return to the next update's first entry.
Pair and sum work intervals per update before reporting median, p99 and maximum.
Check the video mode's refresh period independently when counting missed refreshes.

For N successive update timestamps, the average update rate is
`cpu_hz * (N - 1) / (last_cycle - first_cycle)` when the counter covers the
elapsed interval. For a transfer benchmark, verify which routine ran and how
many bytes it transferred. Keep transfer size and mechanism consistent unless
one is the variable under test. Report emulator timing as such. Physical-board
comparisons also need the board clock, firmware and peripherals.

Use `-v` repeatedly for more emulator diagnostics. If advertised by `--help`,
`--profile` logs aggregate renderer measurements. For controlled comparisons,
`--no-dma` disconnects the ZVB DMA bus handshake and `--no-scanline-rendering`
selects whole-frame rendering in builds that support it. Record these settings
alongside the guest measurements.
