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

Get the `latest` snapshot release at: <https://github.com/X65/emu/releases>

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

## Testing

Tests are built as part of the normal CMake build and run with CTest:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

Run a single suite with `-R`, e.g. `ctest --test-dir build -R ArgsTest`.

The tests use [doctest][4], so you can also run a suite's binary directly to
filter individual cases:

    cmake --build build --target argstest
    build/src/tests/argstest --test-case="*crt*"
    build/src/tests/argstest --list-test-cases

[4]: https://github.com/doctest/doctest

## Running

Linux

    > build/emu --help
    Usage: emu [OPTION...] [ROM.xex]

    > build/emu roms/SOTB.xex

Windows

    > build/emu.exe roms/SOTB.xex

Options use the GNU `--option` style; run `emu --help` for the full list.

### Headless scripting

`--script FILE` drives the machine from a small line-oriented script instead
of the keyboard: advance frames, feed joystick lines, take PNG screenshots,
dump or check memory, print CPU/CGIA state, trace instructions, stop at an
address. Emulation runs at a deterministic 60 Hz (several frames per host
frame), a failed check exits with code 1, and `exit` ends the run, so scripts
double as CI smoke tests. Combine with `--disable-gui` and `xvfb-run` for a
fully headless run. `--screenshot FILE [--frames N]` is a shortcut for
`run N` / `shot FILE` / `exit`.

    > cat drive.scr
    run 60
    joy up left
    run 120
    regs
    shot "turning.png"
    dump 0xD000 32
    exit 0
    > xvfb-run -a build/emu --disable-gui --script drive.scr roms/game.xex

The verbs are documented in `src/script.h`.

### Opcode Breakpoints

The emulator supports opcode based breakpoints, if an specified opcode is executed, the emulator will stop. Possible breakpoint values are EA (NOP) 42 (WDM #xx) and B8 (CLV).

    > build/emu --break EA roms/SOTB.xex

### WASM URL arguments

The web build has no command line, so arguments come from the page URL query
string. `file=VALUE` becomes the positional ROM argument,
and every other token maps to a long option (only the `--option[=value]` form is
supported, but the `--` prefix may be omitted). For example:

    emu.html?file=roms/SOTB.xex&crt=1,2,3&fullscreen

is equivalent to the native command line:

    build/emu --crt 1,2,3 --fullscreen roms/SOTB.xex
