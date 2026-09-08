# Emulator script fixture

`smoke.xex` is committed so ordinary builds and CTest need no assembler.
The 65816 guest starts in emulation mode at `$2000`, records 32 hardware RNG
bytes at `$0320`, writes `$A5` at `$0300`, then loops at `$2080` copying GPIO
joystick port zero to `$0301`.

To regenerate from the repository root, using cc65's ca65/ld65 and Python 3:

```sh
mkdir -p build/emu-smoke-regen
ca65 src/tests/fixtures/emu-smoke/smoke.s -o build/emu-smoke-regen/smoke.o
ld65 -C src/tests/fixtures/emu-smoke/smoke.cfg build/emu-smoke-regen/smoke.o -o build/emu-smoke-regen/smoke.bin
python3 roms/src/mkxex.py build/emu-smoke-regen/smoke.bin build/emu-smoke-regen/smoke.xex --org 0x2000
cmp src/tests/fixtures/emu-smoke/smoke.xex build/emu-smoke-regen/smoke.xex
```

After an intentional source change, copy the regenerated XEX over the fixture
and rerun `ctest --test-dir build -R EmuScript --output-on-failure`.

`smoke.scr` checks loading, bounded execution, and joystick press/release.
`seed.scr.in` is instantiated twice with separate binary dump paths.
`failure.scr` intentionally fails; the CMake driver checks both exit code 1
and the specific memory assertion diagnostic. Each test uses its own build
subdirectory, a virtual display, software GL and `alsa-null.conf` as its
process-local ALSA configuration.

The Linux GUI tests reuse this XEX and drive the real window with xdotool. The
joystick case reads `$0300/$0301` through the emulator's existing DAP TCP service.
The window-lifecycle case runs Openbox inside its private Xvfb display so
fullscreen requests have a window manager to process them.
