# X65 emulator

This is Emu <img src="emu.gif" alt="Emu"> The [X65 Computer][1] Emulator.

Emu is based on [chip emulators][2] by Andre Weissflog.

> The USP of the chip emulators is that they communicate with the outside world
> through a 'pin bit mask': A 'tick' function takes an uint64_t as input
> where the bits represent the chip's in/out pins, the tick function inspects
> the pin bits, computes one tick, and returns a (potentially modified) pin bit mask.
>
> A complete emulated computer then more or less just wires those chip emulators
> together just like on a breadboard.

[1]: https://x65.zone/
[2]: https://github.com/floooh/chips

## Dependencies

Fedora:

    dnf install libX11-devel libXi-devel libXcursor-devel mesa-libEGL-devel alsa-lib-devel libunwind-devel

Ubuntu:

    apt install libx11-dev libxi-dev libxcursor-dev libegl1-mesa-dev libasound2-dev libunwind-dev

## Download

Get the `latest` snapshot release at: https://github.com/X65/emu/releases

## Build

[![CMake on multiple platforms](https://github.com/X65/emu/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/X65/emu/actions/workflows/cmake-multi-platform.yml)

Build using CMake and a modern C/C++ compiler.

> [!TIP]
> This repository uses submodules.
> You need to do `git submodule update --init --recursive` after cloning
> or clone recursively.

### TL;DR

    git clone --depth=1 --recursive --shallow-submodules https://github.com/X65/emu.git
    cd emu
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    build/emu --help

### WASM

Install [Emscripten][3] toolchain. Next, run the following commands:

    mkdir wasm
    cd wasm
    emcmake cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build .

[3]: https://emscripten.org/docs/getting_started/downloads.html

## Running

Linux

    > build/emu --help
    Usage: emu [OPTION...] [ROM.xex]

    > build/emu roms/SOTB.xex

Windows

    > build/emu.exe file=roms/SOTB.xex

### Opcode Breakpoints

The emulator supports opcode based breakpoints, if an specified opcode is executed, the emulator will stop. Possible breakpoint values are EA (NOP) 42 (WDM #xx) and B8 (CLV).

    > build/emu.exe file=roms/SOTB.xex break=EA
