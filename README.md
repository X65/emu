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

The web build plays audio through a Wasm AudioWorklet, which runs on the browser's
real-time audio thread rather than the main one. That takes shared memory, which
browsers only grant to a cross-origin isolated page, so the server has to send:

    Cross-Origin-Opener-Policy: same-origin
    Cross-Origin-Embedder-Policy: require-corp

Without those headers the failure is total rather than silent -- creating the shared
`WebAssembly.Memory` throws and the emulator never starts. `python -m http.server`
does not send them; for local testing use:

    tools/serve_wasm.py wasm
    # http://127.0.0.1:6816/emu.html?file=roms/buzzer.xex

[3]: https://emscripten.org/docs/getting_started/downloads.html

## Testing

Tests are built as part of the normal CMake build and run with CTest:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

`X65Test` covers execution tick accounting and frame publication, CPU/direct
memory routing, timer IRQ and CGIA VBI NMI delivery, SGU inspection and PCM
snapshot continuation, and local seeded RAM/RNG repeatability. It uses the same
machine sources as the application. The existing CPU conformance runners remain
part of CTest; the optional SingleStepTests corpus is not downloaded by the build.

On Linux, CMake also registers `EmuScriptSmoke`, `EmuScriptSeedRepeatability`, and
`EmuScriptCheckFailure` when `xvfb-run` is available. Install Xvfb, xauth, and Mesa
software rendering support before configuring. These tests run the real executable
with a virtual display and a process-local ALSA null sink. Each invocation has a
30-second timeout; each CTest test has a 90-second timeout. Windows runs the portable
native suites. See [fixture regeneration instructions](src/tests/fixtures/emu-smoke/README.md);
normal builds consume the committed XEX and need no assembler.

When xdotool and Python 3 are also available, CMake registers
`EmuGuiJoystickInput`. It sends host W/A/Z key events and reads the fixture's
guest-visible joystick byte through DAP, covering the X11-to-Sokol input path.
With xprop and Openbox, CMake also registers `EmuGuiWindowLifecycle`, which
verifies window creation, title and geometry, fullscreen round trips, debug-UI
hide/show, and a clean Ctrl+Q exit. Openbox supplies the EWMH fullscreen behavior
that a bare Xvfb server lacks.

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
of the keyboard: advance frames, feed joystick lines and gamepad reports, take
PNG screenshots, dump or check memory, print CPU/CGIA state, trace
instructions, stop at an address. Emulation runs at a deterministic 60 Hz (several frames per host
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

`joy [1|2] ...` drives either DE-9 joystick port; the two are independent and
hold at the same time. Beyond those two, players come from the USB HID
gamepads, which are normally fed only by real SDL devices, so
`pad <1..4> [button ...]` injects a report into one of the four HID slots and
`pad <n> off` hands the slot back to whatever is plugged in:

    joy 1 up
    joy 2 down b
    pad 3 right a
    run 120
    joy 2 none
    pad 3 off

The verbs are documented in `src/script.h`. `peek` and `dump` use debugger-style
inspection: timer interrupt status and SGU service status remain pending, and SGU
sample-data reads preserve the sample offset. CPU bus reads still acknowledge
status and advance SGU sample offsets. RIA FIFO/API-stack inspection returns `$FF`.
Other read effects, including consuming hardware RNG bytes, remain unchanged.
Direct debugger/loader access also bypasses expansion-window routing.

`--seed N` optionally seeds randomized RAM and the hardware RNG. Values are unsigned
decimal or `0x`/`0X` hexadecimal through `4294967295`; leading-zero values are decimal,
and zero is a valid supplied seed. Repeated options use the last valid value.
Full initialization/reboot restarts the seed; ordinary reset continues the stream.
`--zero-mem` zeros RAM without changing the seeded guest RNG sequence.

Repeatability requires the same C runtime, initialization options, and inputs:
C runtimes may produce different `rand()` sequences, and unrelated `rand()` calls
share the stream. Snapshot restoration does not rewind RNG state. The SGU snapshot
test covers PCM playback continuation within one executable, not complete machine
replay or restoration of the host audio resampler. Omitting `--seed` retains the
existing initialization behavior.

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
