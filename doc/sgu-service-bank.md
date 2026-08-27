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

   ```
   $00-$03  MAGIC       'S','G','U','1'                    read-only
   $04      VER_MAJOR   $01                                read-only
   $05      VER_MINOR   $00                                read-only
   $06-$0D  UNIQUE_ID   8 bytes, RP2350 board id on hw     read-only
   $0E      PCM_BANKS   count of 64 KB PCM banks          read-only
   $0F      SVC_BANKS   additional service banks ($00 now) read-only
   $10      STATUS      b0 = CLIP                          read-to-clear
   $11-$17  reserved    future status registers
   $18      CHIP_RESET  write $Ax                          write-only
   $19-$1B  reserved    future control registers
   $1C-$1F  sample window
   $20      MASTER_VOL
   $21-$3E  reserved
   ```

   The identification block sits at `$00..$0F`, the status block starts at `$10` and the control block at `$18`; the gaps between them are reserved so either block can grow without moving anything. In particular `STATUS` and `CHIP_RESET` are deliberately not adjacent — more status registers are expected, including ones that are *not* self-clearing.

   - `$00..$03` — **MAGIC**, the ASCII bytes `SGU1`. The chip-detection handshake.
   - `$04` / `$05` — **version** major / minor. There is no patch byte.
   - `$06..$0D` — **UNIQUE_ID**, the 8-byte RP2350 board id (`pico_get_unique_board_id`, `PICO_UNIQUE_BOARD_ID_SIZE_BYTES` = 8) on hardware. **The emulator reports all zeros**: it has no board, and silicon never reports an all-zero id, so all-zero is how software tells it is talking to an emulated chip.
   - `$0E` — **PCM_BANKS**, the number of 64 KB PCM banks actually backed. A discovery register tells the truth about the build it runs on: `SGU1_PCM_BANKS` = 4 in the emulator, 1 on hardware (`pcm_mem[SGU_PCM_BANK_SIZE]`).
   - `$0F` — **SVC_BANKS**, service banks beyond `$FF`. `$00` today; the anticipated `$FE` bank carrying DSP registers and programming would make it 1.
   - `$10` — **STATUS**, read-to-clear. Bit 0 is `CLIP`: the output stage saturated at least once since the last read. `SGU_GetFlags` is self-clearing and expects a single reader, so the wrapper's per-sample tick is that reader: it takes the core's status word once per generated sample and accumulates it into a host-side latch. It accumulates the *whole* word rather than the bits it recognises — anything not passed along is lost there with nothing to point at the loss, so a flag added to the core later still reaches the guest. The latch is the core's width, not the register's, and a read of `$10` clears only the bits it actually returned; a flag above bit 7 survives for whichever of `$11..$17` eventually exposes it. Writes are ignored. Note the debugger's `mem_rd` path also lands here, so parking a memory editor on this address eats the guest's status bits, exactly as a debugger read would on hardware — the emulator's own clip indicator is driven by a separate monotonic counter and is unaffected either way.
   - `$18` — **CHIP_RESET**, write-only (reads `$00`). See below.
   - `$1C` / `$1D` — sample offset low / high byte (read/write, plain readback).
   - `$1E` — sample bank number (read/write; store the byte verbatim and address PCM as `bank * 64 KB + offset`, reading `$00` and dropping writes past the end of the backed memory — see `$0E` for how much that is).
   - `$1F` — sample data port: **write** stores to the chip PCM RAM at the current offset, **read** returns the byte at the current offset; **both auto-increment `$1C/$1D`** (16-bit wrap `$FFFF → $0000`; the bank byte is NOT auto-bumped — confirm with firmware).
   - `$20` — **master volume**, applied by the post-mixing DSP to the final stereo output (composes with the existing `magnitude` scale, sgu1.h:106). **Reset = 0, i.e. muted** — see the note below. **Mirror the exact volume law from the firmware DSP source** (the SGU-1 is firmware on a microcontroller — locate the mixer master-volume application there); if genuinely undocumented, implement provisional linear `value/255` unity-at-`$FF` and mark it in a comment.
   - All other service offsets: reserved — writes ignored, reads `$00` — until the firmware documents them (chip service data / DSP params live here; do not invent).
4. **CHIP_RESET (`$18`) encoding:** the high nybble must be the magic `$A` or the write is ignored entirely, so a stray store cannot silence the chip. The low nybble is a bitmask of reset domains:

   | bit | domain | resets |
   |-----|--------|--------|
   | 0 | VOICES | channel register file, operators, envelopes, per-channel DSP (filters, sweeps, phase/PCM accumulators) |
   | 1 | TIMEBASE | sample / envelope / LFO counters and the LFO noise LFSR (reseeded, not zeroed) |
   | 2 | MIX | output mix, DC-removal filter state, and the `STATUS` latch |
   | 3 | SVC | the service registers below: sample offset, sample bank, **master volume → 0 (muted)** |

   `$A0` is a no-op. `$A7` is a full core reset — byte for byte what `SGU_Reset()` does. `$AF` is core plus service registers, what `sgu1_reset()` does.

   Bits 0–2 name domains of the SGU-1 softcore, which knows nothing of this register: the wrapper maps them onto `SGU_RESET_VOICES` / `_TIMEBASE` / `_MIX` and calls `SGU_RequestReset()`. That only *latches* the request; the core performs it in `SGU_NextSample_Setup()`, at the next sample boundary, before any channel work is dispatched. Resetting from the bus write would tear the register file out from under a render already in flight on the other core. Bit 3 is host-side state touched only by this bus, so it resets straight away.

   Two deliberate exclusions:

   - **PCM sample memory is never cleared by any reset.** It is host-loaded data rather than chip state, and a 64 KB memset inside the 48 kHz sample deadline (~20.8 µs) would overrun the hardware's render budget.
   - **The SVC bit does not clear the channel select at `$3F`.** The select is the register *window*, shared by every bank, not service state — clearing it would deselect the service bank out from under the very sequence issuing the reset. Only a chip reset clears it.

5. **Reset (`sgu1_reset`):** select 0, sample offset 0, sample bank 0, **master volume 0 (muted)**.
   The chip comes up silent by design: master volume gates the entire mix, and audio hardware must not blast
   the user with whatever the channel register file happens to power up holding. Unmuting is the operating
   system's job, once it has put the channels into a known state. Do **not** "fix" this to full volume —
   a bare-metal program that wants sound has to raise `$20` itself, exactly as it would on hardware.
6. **Register dump compatibility:** `sgu1_dump_frame` output (the golden regdump format) must remain channel-registers-only — service state stays out of the dump so existing A/B comparisons are unaffected.

**Acceptance tests** (unit-level against `sgu1_reg_read/write`, no UI needed):

- Upload: select `$FF`, write `$1E=0`, `$1C/$1D = $34/$12`, write N bytes to `$1F` → PCM RAM `$1234..` holds them, offset reads back as `$1234+N`; read-back through `$1F` returns the same bytes and increments; offset wraps `$FFFF→$0000`.
- Channel isolation: snapshot the channel register file, do arbitrary service-bank traffic (including select `$FF` itself), assert the register file is byte-identical — this is the aliasing regression guard.
- Select readback: write `$FF` → read `$3F` = `$FF`; write `3` → read = `3`; reserved select `9` → channel file untouched, reads `$FF`.
- Master volume: render the same tone at full vs half `$20` → output RMS follows the implemented law.
- Identification: `$00..$03` read `SGU1`, `$04`/`$05` read `$01`/`$00`, `$06..$0D` read zero, `$0E` reads `SGU1_PCM_BANKS`, `$0F` reads `$00`; every byte of the block ignores writes.
- STATUS: with a clip latched, `$10` reads bit 0 set and the next read returns `$00`; writes never set a bit.
- STATUS accumulation: several clips before any read coalesce into one latch while the host-side clip counter counts each; a status bit the wrapper does not special-case still reaches `$10` and does not count as a clip; a bit above bit 7 outlives a read of `$10`.
- Inspection: peeking the service bank reports the same values with no side effects — the sample port does not advance and the status latch is not cleared.
- CHIP_RESET: `$00`/`$07`/`$0F`/`$5A`/`$B7`/`$FF` request nothing; `$A0` requests nothing; `$A1`/`$A2`/`$A4` request one domain each; `$A7` requests all three. `$18` reads `$00`.
- CHIP_RESET SVC bit: `$A8` zeroes offset/bank/master volume, requests no core reset, and leaves the select at `$FF`.
- Reserved offsets `$11..$17`, `$19..$1B`, `$21..$3E` read `$00` and ignore writes.
- End-to-end: a small test program (or the SGM `.xex` from S6) uploads a sample via the port and plays a PCM voice audibly.
