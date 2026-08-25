#pragma once
/*
    script.h -- headless test/debug scripting for the X65 emulator

    A tiny line-oriented command language that drives the emulated machine
    without a keyboard or a human looking at the window: advance frames, feed
    joystick input, take PNG screenshots, dump/verify memory, print CPU/CGIA
    state, trace instructions and stop at addresses. Meant for CI smoke tests
    and for agents debugging programs from a terminal.

    Verbs (one per line, `#` starts a comment; numbers accept 0x/$ hex):

      run [frames]              advance N emulated frames (default 1)
      until <addr> [frames]     run until an opcode is fetched from addr (24-bit),
                                give up after N frames (default 600)
      joy [up|down|left|right|a|b|c|d|none ...]   set joystick-1 lines
      shot "file.png" [full]    write the display; half-res (384x240) unless "full"
      crc                       print CRC-32 of the display
      expect-crc <hex>          fail unless the display CRC matches
      dump <addr> [count] ["file"]   hex-dump memory (or write it raw to file)
      peek <addr> <byte>...     fail unless memory matches
      poke <addr> <byte>...     write memory
      regs                      print CPU registers
      cgia                      print CGIA registers and internal plane state
      trace [n]                 print the next N executed instructions (default 32)
      echo "text"               print text
      exit [code]               leave the emulator with an exit code

    A failing check prints the script line and exits with code 1.
*/

#include <stdbool.h>
#include <stdint.h>

#include "systems/x65.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load a script file ("-" = stdin). Returns false if it cannot be opened.
bool script_load(const char* path);
// Arm the --screenshot/--frames shortcut: run `frames` (NULL = default) frames,
// write the PNG, then exit. Exits with a message if either argument is unusable.
void script_screenshot(const char* path, const char* frames);
// True while a script is armed and still has work to do.
bool script_running(void);
// One host frame's worth of scripted execution: runs the machine itself
// (several emulated frames per call while waiting) and executes commands
// until the next one that has to wait. A bad line or a failed check prints
// the offending line and exits with code 1.
void script_task(x65_t* sys);

#ifdef __cplusplus
}
#endif
