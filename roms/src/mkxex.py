#!/usr/bin/env python3
"""Wrap a raw 6502/65816 binary into an X65 .xex image.

The loader (x65_quickload_xex) expects a $FFFF magic word followed by one or
more segments of <start:u16><end:u16><data...> (little-endian). Writing the
reset vector at $FFFC/$FFFD makes the emulator auto-run the program.

Usage:
    mkxex.py IN.bin OUT.xex [--org ADDR] [--title TEXT]

    --org    load/run address of IN.bin (default: 0x0200)
    --title  optional label stored at $FC00 (skipped by the loader)
"""
import argparse
import struct


def seg(start: int, data: bytes) -> bytes:
    end = start + len(data) - 1
    return struct.pack("<HH", start, end) + data


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("infile")
    ap.add_argument("outfile")
    ap.add_argument("--org", default="0x0200", type=lambda s: int(s, 0))
    ap.add_argument("--title", default=None)
    args = ap.parse_args()

    code = open(args.infile, "rb").read()

    out = b"\xff\xff"  # magic header
    if args.title:
        out += seg(0xFC00, args.title.encode())  # title chunk, loader skips $FC00
    out += seg(args.org, code)  # the program
    out += seg(0xFFFC, struct.pack("<H", args.org))  # reset vector -> auto-run

    open(args.outfile, "wb").write(out)
    end = args.org + len(code) - 1
    print(f"wrote {args.outfile}: {len(out)} bytes; "
          f"code {len(code)} @ ${args.org:04X}-${end:04X}, reset=${args.org:04X}")


if __name__ == "__main__":
    main()
