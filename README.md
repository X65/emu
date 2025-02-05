# X65 emulator

This is Emu <img src="emu.gif" alt="Emu"> The [X65 Computer][1] Emulator.

Based on [chips emulators][2] by Andre Weissflog.

> The USP of the chip emulators is that they communicate with the outside world
> through a 'pin bit mask': A 'tick' function takes an uint64_t as input
> where the bits represent the chip's in/out pins, the tick function inspects
> the pin bits, computes one tick, and returns a (potentially modified) pin bit mask.
>
> A complete emulated computer then more or less just wires those chip emulators
> together just like on a breadboard.

This emulator started as a hard fork of C64 emulator example
and it shows in places - it is still a work-in-progress.
Machine is being gradually transitioned to X65 emulator,
by replacing chips modules and "rewiring" the system shell.

[1]: https://x65.zone/
[2]: https://github.com/floooh/chips

## Dependencies

Fedora:

    dnf install libX11-devel libXi-devel libXcursor-devel mesa-libEGL-devel alsa-lib-devel

Ubuntu:

    apt install libx11-dev libxi-dev libxcursor-dev libegl1-mesa-dev libasound2-dev

## Build

[![CMake on multiple platforms](https://github.com/X65/emu/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/X65/emu/actions/workflows/cmake-multi-platform.yml)

Build using CMake and a modern C/C++ compiler.

### WASM

Install [Emscripten][3] toolchain. Next, run the following commands:

    mkdir wasm
    cd wasm
    emcmake cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build .

[3]: https://emscripten.org/docs/getting_started/downloads.html
