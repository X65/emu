# Appendix A — SGU-1 service bank ($FF): emulator implementation requirements

Self-contained work order for the **emu repo** (`/home/smoku/devel/X65/devel/emu`, a separate repo — nothing in sgu-tracker changes). The emu fully implements the SGU-1 core and channels but not the hardware's service bank; the SGM plan's device-stage verification (S6/S7) depends on it. Files: `src/chips/sgu1.h` / `src/chips/sgu1.c` (the `x65.c` address decode already routes the whole `$FEC0..$FEFF` window to `sgu1_reg_read/write` with `reg = addr & SGU1_ADDR_MASK` — no system-level change needed).

**Current behavior (and the bug to avoid):** `sgu1_reg_write` treats reg `$3F` as the channel select and otherwise writes `(selected_channel % SGU_CHNS) << 6 | reg` into the channel register file (sgu1.c:87-97; read mirror at :76-84). The modulo means select = `$FF` today **aliases to channel 3** (255 mod 9) — service-bank traffic would silently corrupt a live channel. Any implementation must first remove that aliasing for out-of-range selects.

**Required semantics:**

1. **Select register (window offset `$3F`):** write stores the value verbatim (`selected_channel` u8, including `$FF`); read returns the last written value. Reset value 0.
2. **Bank routing:** offsets `$00..$3E` route by `selected_channel`:
   - `0..SGU_CHNS-1` → channel registers, **byte-identical to today** (regression requirement).
   - `$FF` → the service bank (below).
   - `9..$FE` → reserved: writes ignored, reads return `$FF`. Never modulo-alias onto a channel. (Confirm intended hardware behavior with firmware; this is the safe default.)
3. **Service bank registers** (new `sgu1_t` state: `svc_sample_offset` u16, `svc_sample_bank` u8, `svc_master_vol` u8):
   - `$1C` / `$1D` — sample offset low / high byte (read/write, plain readback).
   - `$1E` — sample bank number (read/write; only bank 0 is backed today — the PCM RAM is 64 KiB; store the byte, address PCM as `offset` within bank 0, ignore non-zero banks for now).
   - `$1F` — sample data port: **write** stores to the chip PCM RAM at the current offset, **read** returns the byte at the current offset; **both auto-increment `$1C/$1D`** (16-bit wrap `$FFFF → $0000`; the bank byte is NOT auto-bumped — confirm with firmware).
   - `$20` — **master volume**, applied by the post-mixing DSP to the final stereo output (composes with the existing `magnitude` scale, sgu1.h:106). **Reset = 0, i.e. muted** — see the note below. **Mirror the exact volume law from the firmware DSP source** (the SGU-1 is firmware on a microcontroller — locate the mixer master-volume application there); if genuinely undocumented, implement provisional linear `value/255` unity-at-`$FF` and mark it in a comment.
   - All other service offsets: reserved — writes ignored, reads `$00` — until the firmware documents them (chip service data / DSP params live here; do not invent).
4. **Reset (`sgu1_reset`):** select 0, sample offset 0, sample bank 0, **master volume 0 (muted)**.
   The chip comes up silent by design: master volume gates the entire mix, and audio hardware must not blast
   the user with whatever the channel register file happens to power up holding. Unmuting is the operating
   system's job, once it has put the channels into a known state. Do **not** "fix" this to full volume —
   a bare-metal program that wants sound has to raise `$20` itself, exactly as it would on hardware.
5. **Register dump compatibility:** `sgu1_dump_frame` output (the golden regdump format) must remain channel-registers-only — service state stays out of the dump so existing A/B comparisons are unaffected.

**Acceptance tests** (unit-level against `sgu1_reg_read/write`, no UI needed):

- Upload: select `$FF`, write `$1E=0`, `$1C/$1D = $34/$12`, write N bytes to `$1F` → PCM RAM `$1234..` holds them, offset reads back as `$1234+N`; read-back through `$1F` returns the same bytes and increments; offset wraps `$FFFF→$0000`.
- Channel isolation: snapshot the channel register file, do arbitrary service-bank traffic (including select `$FF` itself), assert the register file is byte-identical — this is the aliasing regression guard.
- Select readback: write `$FF` → read `$3F` = `$FF`; write `3` → read = `3`; reserved select `9` → channel file untouched, reads `$FF`.
- Master volume: render the same tone at full vs half `$20` → output RMS follows the implemented law.
- End-to-end: a small test program (or the SGM `.xex` from S6) uploads a sample via the port and plays a PCM voice audibly.
