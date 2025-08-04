# Zeal Native Emulator

This project is a software emulator for Zeal 8-bit Computer: a homebrew 8-bit computer based on a Z80 CPU.

The emulator is written in C, and runs natively on desktop platforms. It features hardware-accelerated rendering for efficient graphics and is designed to precisely replicate the behavior of the original hardware.

Whether you're developing software for Zeal 8-bit Computer or just using it to execute existing software, this emulator provides a fast and flexible environment for running and testing your code.

## Why this project?

An emulator for the Zeal 8-bit Computer already exists: it's called [Zeal-WebEmulator](https://github.com/Zeal8bit/Zeal-WebEmulator). That emulator is also public and open-source. To make it easily accessible without requiring any installation, it was developed in JavaScript and runs entirely in the web browser. However, due to this implementation choice, it struggles to maintain full speed and deliver a native-like experience when running Z80 CPU-intensive programs.

That's where the **Zeal Native Emulator** comes in!
This project is written entirely in C, using Raylib for 2D rendering and Nuklear for UI. It takes advantage of hardware acceleration through OpenGL and GLSL shaders, delivering a fast and smooth experience that works just like the real hardware. Even if the emulated programs are very demanding or CPU-intensive, the emulator always runs at full speed!

Thanks to its native implementation, this emulator is also much more portable, it can be adapted to other platforms including (but not limited to) WASM and Android. See the [Ports](#ports) section for more details.

Additionally, because the emulator runs directly on the host system, it's much easier to work with local files, for example, using files to emulate EEPROMs, microSD/TF cards, CF cards, and more. The emulator supports a several command-line options, making it ideal for automation.

## Build

### Requirements

To compile the emulator, you will need the following software:

* **Meson** build system
* **GCC** or **Clang** compiler
* Raylib **5.5** or newer (already included in the repository for the WebAssembly version).

### Hardware Requirements

* For the native (desktop) version, your system must support OpenGL 3.3 Core since the emulator uses modern GLSL shaders.
* For the WebAssembly (WASM) version, the browser must support WebGL2, which corresponds to OpenGL ES 3.0.

### Native compilation

To compile the emulator natively to your operating system, use the following commands:

```
meson setup build
cd build
meson compile
```

You will then have a `zeal.elf` binary that you can run. You can check all the parameters via `./zeal.elf --help`.

### WebAssembly compilation

To compile to WASM, use the following commands:

```
meson setup build-wasm --cross-file wasm-cross.build
cd build-wasm
meson compile
```

Keep in mind that this build will use the Raylib 5.5 release that is present at the root of the project, in `raylib/wasm`. If you wish to override this library and use your own version

### Compilation options

All the compilation options are listed in the `meson_options.txt` file. Check it to see all the available options.

For example, if Raylib is not installed in the system's default path, you can pass its location to the Meson command line by adding `-Draylib_path=/path/to/your/raylib`. For native compilation, the command would be:

```
meson setup build -Draylib_path=/path/to/your/raylib
```

### Clean

To clean the build, you can either use:

```
meson compile --clean
```

Or simply delete the build directory.


## Supported Features

Currently, the following features from Zeal 8-bit Computer are emulated:

* Z80 CPU (from [Superzazu](https://github.com/superzazu/z80), modified by @Zeal8bit)
* Z80 PIO: all modes supported, both port A and B. Implementation is independent of connected devices.
* 22-bit MMU
* 256KB NOR flash (read-only currently)
* 512KB RAM
* Zeal 8-bit Video Card, **firmware v1.0.0** :
    * All text, graphics, bitmap modes, including sprites, palette, layers, etc...
    * Text controller
    * DMA controller
    * Audio controller (all voices, including the sample table)
    * CRC controller
    * SPI controller
* PS/2 Keyboard, with interrupts
* UART TX: prints to stdout
* I2C: bus emulated, supporting write/read/write-read operations
    * DS1307 RTC
    * 24C512 (64KB) EEPROM is emulated
* microSD/TF card support
* HostFS support to access a directory of the desktop computer in the emulator directly

The emulator also implement a debugger, with the following features:

* Breakpoints
* Step/stop/restart/continue
* Disassembler: when the emulator reaches a breakpoint, the code at program counter is disassembled. Clicking on one of the instructions will toggle a breakpoint.
* Load a map file generated from `z88dk-z88dk` to view symbols in the assembly view when doing step-by-step debugging
* Set breakpoints with either a hexadecimal PC address or a symbol (from the map file)
* View RAM memory content at any virtual address, updated in live
* View VRAM memory content, including the current palette, tileset, tilemaps and font


## Ports

Raylib supports a wide range of platforms, which makes the Zeal Native Emulator easy to port to other targets.

### WebAssembly

To compile the emulator to WebAssembly, follow the steps described above in [WebAssembly compilation](#webassembly-compilation).

After compilation, the `build-wasm` directory will contain an `index.html`, `index.css`, and all other necessary files.

**Note:** You cannot simply open the `index.html` file directly in your browser to run the emulator. You need to start a local web server to serve the files properly. This can be as simple as serving the current directory. For example, the easiest way is:

```
cd build-wasm
python3 -m http.server
```

Then, open http://0.0.0.0:8000/ in any modern web browser (Chrome-based browsers are recommended).

### Android (WIP)

This port is still in progress. It requires the Android NDK in order to compile the C code for the Android execution environment.

## Contributing

Anyone can contribute to this project, contributions are welcome!

Feel free to fix any bug that you may see or encounter, implement any feature that is present in the TODO list or a new one that you find useful or important.

To contribute:

* Fork the Project
* Create your feature Branch (optional)
* Commit your changes. Please make a clear and concise commit message (*)
* Push to the branch
* Open a Pull Request

(*) A good commit message is as follows:

```
Module: add/fix/remove a from b

Explanation on what/how/why
```

For example:

```
hw/zvb: implement 320x240 text-mode

It is now possible to switch to 320x240 text-mode and display text.
```

## License

Z80 CPU C implementation from [Superzazu](https://github.com/superzazu/z80) is distributed under the MIT licence.

All the other files are distributed under the Apache 2.0 License. See LICENSE file for more information.

You are free to use it for personal and commercial use, the boilerplate present in each file must not be removed.

## Contact

For any suggestion or request, you can contact me at contact [at] zeal8bit [dot] com

For feature requests, you can also open an issue or a pull request.