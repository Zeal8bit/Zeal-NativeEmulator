# Zeal Native Emulator

This project is a software emulator for Zeal 8-bit Computer: a homebrew 8-bit computer based on a Z80 CPU.

The emulator is written in C, and runs natively on desktop platforms. It features hardware-accelerated rendering for efficient graphics and is designed to precisely replicate the behavior of the original hardware.

Whether you're developing software for Zeal 8-bit Computer or just using it to execute existing software, this emulator provides a fast and flexible environment for running and testing your code.

## Build

### Requirements

To compile the emulator, you will need the following software:

* **Meson** build system
* **GCC** or **Clang** compiler
* Raylib **5.5** or newer (already included in the repository for the WebAssembly version)

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

### Clean

To clean the build, you can either use:

```
meson compile --clean
```

Or simply delete the build directory.


## Supported Features

Currently, the following features from Zeal 8-bit Computer are emulated:

* Z80 CPU (from [Superzazu](https://github.com/superzazu/z80), modified by myself)
* Z80 PIO: all modes supported, both port A and B. Implementation is independent of connected devices.
* 22-bit MMU
* 256KB NOR flash (SST39SF020)
* 512KB RAM
* Zeal 8-bit Video Card, **firmware v0.2.2** :
    * All text, graphics, bitmap modes, including sprites, palette, layers, etc...
    * Text controller
    * DMA controller
    * Audio controller
    * CRC controller
    * SPI controller
* PS/2 Keyboard, with interrupts
* UART TX: it is possible to print bytes
* I2C: bus emulated, supporting write/read/write-read operations
    * DS1307 RTC
    *  24C512 EEPROM is emulated
* microSD/TF card support
* HostFS support to access a directory of the desktop computer in the emulator directly
