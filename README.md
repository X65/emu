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

Gamepads stay invisible to a web page until someone presses a button on one --
the Gamepad API hides them from a page that has not been played with, so a pad
plugged in before the page loaded shows up on the status line only after its
first button press, and the browser then reveals every pad at once.
Browser pads use raw joystick reports, preserving the firmware's button order.

[3]: https://emscripten.org/docs/getting_started/downloads.html

## Testing

Tests are built as part of the normal CMake build and run with CTest:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

`X65Test` covers execution tick accounting and frame publication, CPU/direct
memory routing, timer IRQ and CGIA VBI NMI delivery, SGU inspection and PCM
snapshot continuation, and local seeded RAM/RNG repeatability. It uses the same
machine sources as the application. On Linux, `CPU816Suite` runs gilyon's 65816
instruction tests and `WaiInterrupt` checks interrupt wakeup from WAI.

The optional SingleStepTests/65816 runner checks CPU state and bus cycles.
Fetch selected opcodes and register the corpus with CTest:

    tools/fetch-sst65816.sh 3d 48 cb
    cmake -S . -B build -DSST65816_DIR="$PWD/sst65816/v1"
    cmake --build build --target sst65816
    ctest --test-dir build -R SST65816 --output-on-failure

Omit the fetch arguments for the full corpus (~3 GB); builds never download it.

On Linux, CMake also registers `EmuScriptSmoke`, `EmuScriptSeedRepeatability`,
`EmuScriptWholeFrame`, and `EmuScriptCheckFailure` when `xvfb-run` is available.
Install Xvfb, xauth, and Mesa software rendering support before configuring.
These tests run the real executable
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
that a bare Xvfb server lacks. GUI tests have a 120-second timeout.

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

`--fill-mem=N` fills RAM with byte `N` before loading the ROM. Values may be
decimal (`--fill-mem=165`) or `0x`/`0X` hexadecimal (`--fill-mem=0xA5`), from
`0` to `255`; leading-zero values are decimal. Repeated options use the last
valid value. Without this option, RAM is randomized. `--fill-mem=0` replaces
the former `--zero-mem` / `-z` option and preserves its zero-filled RAM behavior.
In the web build, use the URL query parameter `fill-mem=N`.

`--fullscreen` starts fullscreen; toggle with Alt+Enter (F11 in the browser).
Fullscreen inhibits the screensaver where supported. The status line shows
DE-9 joystick input and merged HID gamepad directions and buttons. The UI uses
the X65 mouse cursor.

### Audio recording

`--wav FILE` records audio at 48 kHz, stereo, 32-bit float, before host resampling.
Recording lasts for the run and works with scripts, including scripted exits.

    build/emu --wav music.wav roms/MontyOnTheRun.xex

### Hardware and debugging

- SGU-1's [service bank](doc/sgu-service-bank.md) provides chip identification,
  four 64 KiB PCM banks, master volume, clip status, and selective resets.
  Emulator reset mutes the mixer; guest code must set `MASTER_VOL` to enable output.
  Snapshots include PCM data.
- Hardware > SGU-1 shows service state, clip events, and reset controls.
  The mixer slider controls master volume; the speaker icon turns red on clipping.
- CGIA supports 1–4 bpp sprites, mirroring, and double width. The CGIA debugger
  decodes sprite formats and colors; the VRAM debugger has a depth selector
  and a 16-entry sprite palette.
- RIA `EXT_IO` maps eight 64-byte chunks at `$FC00–$FDFF`: set bits select RAM,
  clear bits select the expansion bus. No expansion cards are emulated;
  bus reads return `$FF`. Debugger and loader access bypass this routing.

### Headless scripting

`--script FILE` drives the machine from a small line-oriented script instead
of the keyboard: advance frames, feed joystick lines and gamepad reports, take
PNG screenshots, dump or check memory, print CPU/CGIA state, trace
instructions, stop at an address. Emulation runs at a deterministic 60 Hz
(several frames per host frame), a failed check exits with code 1, and `exit`
ends the run, so scripts
double as CI smoke tests. Combine with `--disable-gui` and `xvfb-run` for a
fully headless run. `--script -` reads stdin.
`--screenshot FILE [--frames N]` is a shortcut for `run N` / `shot FILE` / `exit`
(default 120 frames). `shot` writes 384×240 PNGs; `shot FILE full` uses full
resolution.

`shot` and `crc` capture what the host is shown, not the raster being drawn.
A script regains control only between fixed slices of emulated time, so a
`run` ends partway into the next frame; the capture is the frame CGIA last
finished, not that frame's top drawn over the next. After an `until` the
machine is stopped where the breakpoint caught it, and the capture is the
raster exactly as it stands there.

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
hold at the same time. Its buttons are `a`, `b`, `x` and `y`. A DE-9 stick
conventionally has one or two buttons, so only A and B are standard; X and Y
name the other two after the gamepad, as the platform's controller example
does, and `c`/`d` are accepted as aliases. Beyond those two, players come from
the USB HID gamepads, which are normally fed only by real SDL devices, so
`pad <1..15> [button ...]` injects a report into one of the fifteen HID slots and
`pad <n> off` hands the slot back to whatever is plugged in. Its `lup`/`rup`
style directions drive the analog sticks' digital encoding, the byte programs
usually merge with the dpad so either input works:

    joy 1 up
    joy 2 down b
    pad 3 right a
    pad 4 lup b
    key w lshift
    run 120
    joy 2 none
    pad 3 off
    key none

`key` holds a set of keyboard keys, named (`w`, `up`, `lshift`, `kp0`, ...) or
as raw USB HID usage ids; like `joy`, each call replaces the set.

The verbs are documented in [src/script.h](src/script.h). `peek` and `dump` use
debugger-style inspection: timer interrupt status and SGU service status remain pending, and SGU
sample-data reads preserve the sample offset. CPU bus reads still acknowledge
status and advance SGU sample offsets. RIA FIFO/API-stack inspection returns `$FF`.
Other read effects, including consuming hardware RNG bytes, remain unchanged.
Direct debugger/loader access also bypasses expansion-window routing.

`vpeek`, `vpoke`, and `vdump` access raw RAM as CGIA sees it, bypassing MMIO
at `$FEC0–$FFFF`. The XEX loader warns when a bank-0 block crosses into this
I/O window, where writes go to registers instead of video memory.

`--seed N` optionally seeds randomized RAM and the hardware RNG. Values are unsigned
decimal or `0x`/`0X` hexadecimal through `4294967295`; leading-zero values are decimal,
and zero is a valid supplied seed. Repeated options use the last valid value.
Full initialization/reboot restarts the seed; ordinary reset continues the stream.
`--fill-mem=N` fills RAM without changing the seeded guest RNG sequence.
Without `--seed`, the first boot chooses a random seed and logs the `--seed=N`
needed to repeat it. Reboots reuse that seed.

Repeatability requires the same C runtime, initialization options, and inputs:
C runtimes may produce different `rand()` sequences, and unrelated `rand()` calls
share the stream. Snapshot restoration does not rewind RNG state. The SGU snapshot
test covers PCM playback continuation within one executable, not complete machine
replay or restoration of the host audio resampler.

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
