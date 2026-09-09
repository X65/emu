# SGU-1 service bank

The SGU-1 exposes one 64-byte register window at `$FEC0..$FEFF`. `$FEFF`
selects which bank occupies `$FEC0..$FEFE`:

| Selector written to `$FEFF` | `$FEC0..$FEFE` |
| --- | --- |
| `$00..$08` | The corresponding SGU-1 channel registers. |
| `$FF` | The service bank documented here. |
| `$09..$FE` | Reserved. Reads return `$FF`; writes have no effect. |

The selector retains the exact byte written to it and reads back through
`$FEFF`. It resets to `$00`. In particular, `$FF` does not alias a channel.

The service bank gives software a stable way to detect the chip, upload PCM
data, control the final mixer gain, and reset selected chip domains. This is
the current emulator interface, version 1.0.

## Register map

Select `$FF`, then access the following offsets through `$FEC0 + offset`.
Unlisted offsets are reserved: reads return `$00` and writes have no effect.

| Offset | Address | Name | Access | Meaning |
| --- | --- | --- | --- | --- |
| `$00..$03` | `$FEC0..$FEC3` | `MAGIC` | R | ASCII `SGU1`. |
| `$04` | `$FEC4` | `VER_MAJOR` | R | `$01`. |
| `$05` | `$FEC5` | `VER_MINOR` | R | `$00`. |
| `$06..$0D` | `$FEC6..$FECD` | `UNIQUE_ID` | R | Eight bytes. The emulator returns all zeroes, which distinguishes it from hardware. |
| `$0E` | `$FECE` | `PCM_BANKS` | R | Number of backed 64 KiB PCM banks: `$04` in this emulator. |
| `$0F` | `$FECF` | `SVC_BANKS` | R | Number of service banks beyond `$FF`: `$00` at present. |
| `$10` | `$FED0` | `STATUS` | R | Read-to-clear status byte. Bit 0, `CLIP`, means the output saturated at least once since the preceding read. |
| `$18` | `$FED8` | `CHIP_RESET` | W | Reset selected domains; see below. Reads return `$00`. |
| `$1C` | `$FEDC` | `SAMPLE_OFF_LO` | R/W | Low byte of the PCM sample offset. |
| `$1D` | `$FEDD` | `SAMPLE_OFF_HI` | R/W | High byte of the PCM sample offset. |
| `$1E` | `$FEDE` | `SAMPLE_BANK` | R/W | PCM bank number. The byte is retained even when it names an unbacked bank. |
| `$1F` | `$FEDF` | `SAMPLE_DATA` | R/W | PCM byte at the selected bank and offset; accesses advance the offset. |
| `$20` | `$FEE0` | `MASTER_VOL` | R/W | Final stereo-mix gain. `$00` is mute and `$FF` is unity gain. The emulator currently uses a linear `value / 255` law. |

The identification block is read-only. `STATUS` and all reserved locations
ignore writes.

`SAMPLE_DATA` advances the 16-bit offset after each read or write, wrapping
from `$FFFF` to `$0000`. It never increments `SAMPLE_BANK`. Accesses to an
unbacked bank return `$00` and discard writes, while still advancing the
offset. PCM bytes are signed sample data but travel over the bus unchanged.

`STATUS` is latched by the audio path, so repeated clips coalesce into one
set `CLIP` bit until software reads it. A CPU read clears only the low status
byte it returns. The emulator keeps the remaining status bits internally for
future status registers.

## Reset

Writing `CHIP_RESET` has an effect only when its high nybble is `$A`. The low
nybble is a domain mask:

| Bit | Value | Domain | Effect |
| --- | --- | --- | --- |
| 0 | `$01` | `VOICES` | Resets channel registers, operators, envelopes, and per-channel DSP state. |
| 1 | `$02` | `TIMEBASE` | Resets sample, envelope, and LFO counters, including the LFO noise generator. |
| 2 | `$04` | `MIX` | Resets the output mixer state and clears the service status latch. |
| 3 | `$08` | `SVC` | Immediately resets the sample offset, sample bank, and master volume. |

`$A0` is a no-op. `$A7` resets all core domains and `$AF` resets the core plus
the service registers. Core resets take effect at the next SGU sample boundary;
the service state is reset immediately. PCM memory is never cleared by either
form of reset. A service reset also leaves `$FEFF` selected as `$FF`, so code
can continue its service-bank sequence.

A full emulator reset selects channel `$00`, clears the service registers, and
therefore starts with `MASTER_VOL = $00`. It also preserves PCM memory. Set a
non-zero master volume after configuring audio to enable output.

## Minimal PCM upload

This writes bytes from `samples` into bank 0 at offset `$1234` and enables the
final mixer. Channel configuration and playback are separate SGU-1 operations.

```asm
        lda #$ff
        sta $feff           ; select service bank
        lda #$00
        sta $fede           ; bank 0
        lda #$34
        sta $fedc           ; offset $1234, low byte
        lda #$12
        sta $fedd           ; offset $1234, high byte

        ldy #0
.upload lda samples,y
        sta $fedf           ; write byte and advance offset
        iny
        cpy #samples_end-samples
        bne .upload

        lda #$ff
        sta $fee0           ; unity master volume
```

## Emulator inspection and snapshots

Debugger and script inspection use a side-effect-free view of this window:
they do not clear `STATUS` or advance `SAMPLE_DATA`. CPU reads retain those bus
effects. Snapshot save/load includes the four PCM banks and reconnects the SGU
core to the restored PCM storage; register dumps intentionally contain channel
registers only, not service state.

The implementation is in `src/chips/sgu1.c` and its executable specification
is `src/tests/sgu1test.cpp`.
