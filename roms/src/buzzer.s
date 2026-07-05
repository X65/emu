; buzzer.xex -- X65 PWM buzzer demo
;
; Plays a repeating rising-pitch siren on the on-board buzzer to show off the
; PWM sound hardware.
;
; Buzzer MMIO registers (bank 0):
;   $FFA8  FREQ_LO  \ 16-bit logarithmic pitch, ~20 Hz .. 20 kHz
;   $FFA9  FREQ_HI  /   f = 20 * 2^(9.9658 * FREQ / 65535)
;   $FFAA  DUTY     duty cycle 0..255 (0 = silent)
;
; The 65C816 boots in 6502 emulation mode, so this is plain 6502 code.
;
; Build -- assemble with xa then wrap into an .xex with mkxex.py
;
;   xa buzzer.s -o buzzer.bin
;   ./mkxex.py buzzer.bin ../buzzer.xex --org 0x0200 --title "Buzzer siren demo"
;
; mkxex.py writes the $FFFF magic word then the program and reset vector so
; the emulator auto-runs it. See mkxex.py for the .xex layout details.
;
; Note -- xa mis-parses a colon in a long comment line as a label, so keep
; comment lines short and colon-free.

FREQ_LO = $FFA8
FREQ_HI = $FFA9
DUTY    = $FFAA

; zero-page var holding the current pitch (FREQ high byte)
freq    = $02

        *=$0200

start
        sei
        cld
        ldx #$ff
        txs

        lda #$80        ; 50% duty -> strong square-wave tone
        sta DUTY
        lda #$00
        sta FREQ_LO     ; low byte stays 0; we sweep via the high byte
        sta freq

loop
        lda freq
        sta FREQ_HI     ; writing FREQ latches the new pitch

        ldy #$30        ; hold this pitch for ~20 ms
dly_y   ldx #$00
dly_x   inx
        bne dly_x
        dey
        bne dly_y

        lda freq        ; step the pitch up; wraps around into a siren
        clc
        adc #$02
        sta freq

        jmp loop
