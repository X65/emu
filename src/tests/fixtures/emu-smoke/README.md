# Emulator script fixture

`smoke.xex` is committed so ordinary builds and CTest need no assembler.
The 65816 guest starts in emulation mode at `$2000`, records 32 hardware RNG
bytes at `$0320`, writes `$A5` at `$0300`, then loops at `$2080` copying GPIO
DE-9 joystick port 1 to `$0301`, the dpad and button0 bytes of USB HID
gamepads 1 and 2 to `$0302`-`$0305`, gamepad 1's sticks byte to `$0309`, DE-9 port 2 to `$0306`, and one keyboard
byte from each key-map page to `$0307`/`$0308`.

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

`smoke.scr` checks loading, bounded execution, joystick press/release on both
DE-9 ports independently including all four buttons and their legacy `c`/`d`
aliases, the `key` verb across both key-map pages, and the
`pad` verb: injection into two HID slots, the connected flag, and handing
a slot back with `pad <n> off`.
`seed.scr.in` is instantiated twice with separate binary dump paths.
`failure.scr` intentionally fails; the CMake driver checks both exit code 1
and the specific memory assertion diagnostic. Each test uses its own build
subdirectory, a virtual display, software GL and `alsa-null.conf` as its
process-local ALSA configuration.

The Linux GUI tests reuse this XEX and drive the real window with xdotool. The
joystick case reads `$0300/$0301` through the emulator's existing DAP TCP service.
The window-lifecycle case runs Openbox inside its private Xvfb display so
fullscreen requests have a window manager to process them.
