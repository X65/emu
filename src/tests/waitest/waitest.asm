; Regression test for WAI interrupt wake handling.
;
; Reproduces the bug fixed in "Fix WAI interrupt wake handling": waking from
; WAI on an IRQ with the I flag clear used to fall straight through into the
; rest of the tick, latching whatever was left on the data bus as an opcode
; and running it, instead of letting the next fetch turn into the interrupt
; sequence. The interrupt was never serviced.
;
; The demo that first showed this has since been rewritten not to use WAI, so
; this stands in for it. Run under cpuemu with an interrupt source:
;
;   cpuemu -a 8000 waitest.bin -o 7f01 -d 7f00 -I 7f02
;
; Ports, matching src/tests/cputest816:
;   $7F00  result byte, FF once every check has passed
;   $7F01  character output
;   $7F02  write N to raise IRQ N cycles from now, 0 to release it
;   $7F03  the same for NMI

.p816

IO_RESULT = $7F00
IO_TX     = $7F01
IO_IRQ    = $7F02

RESULT_OK = $FF

; far enough ahead that the line goes active while the CPU is parked
IRQ_DELAY = 20

.segment "ZEROPAGE"
handler_ran:  .res 1
saved_pc:     .res 2

.segment "CODE"

.a8
.i16
reset:
    clc
    xce                 ; native mode
    sei
    rep #$10            ; 16-bit X/Y
    sep #$20            ; 8-bit A
    ldx #$01FF
    txs
    lda #$00
    pha
    plb                 ; data bank 0
    stz IO_RESULT

    ;-----------------------------------------------------------------
    ; With I clear, an IRQ must both wake the CPU and be serviced, and
    ; the address pushed must be the instruction following the WAI.
    ;-----------------------------------------------------------------
    stz handler_ran
    lda #IRQ_DELAY
    sta IO_IRQ
    cli
    wai
after_wai:
    sei
    lda handler_ran
    beq fail_not_serviced       ; the bug: woke up, never vectored
    ldx saved_pc
    cpx #after_wai
    bne fail_wrong_pc           ; vectored, but from the wrong address

    ;-----------------------------------------------------------------
    ; With I set, the same IRQ must wake the CPU without being serviced:
    ; execution resumes at the instruction after the WAI.
    ;-----------------------------------------------------------------
    stz handler_ran
    lda #IRQ_DELAY
    sta IO_IRQ
    sei
    wai
    stz IO_IRQ                  ; release before anything can enable I
    lda handler_ran
    bne fail_serviced_masked

    ldx #txt_ok
    jsr write_text
    lda #RESULT_OK
    sta IO_RESULT
    stp

fail_not_serviced:
    ldx #txt_not_serviced
    bra fail
fail_wrong_pc:
    ldx #txt_wrong_pc
    bra fail
fail_serviced_masked:
    ldx #txt_serviced_masked
fail:
    jsr write_text
    stz IO_RESULT
    stp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Native mode IRQ. The sequence pushed PB, PCH, PCL then P, so the return
; address sits just above the status byte.
irq_handler:
    lda $02,s
    sta saved_pc
    lda $03,s
    sta saved_pc+1
    lda #$01
    sta handler_ran
    stz IO_IRQ          ; release the line, or we re-enter immediately
    rti

; Anything else arriving here means the CPU went somewhere unintended.
trap:
    ldx #txt_trap
    jsr write_text
    stz IO_RESULT
    stp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 0:x = text (null-terminated)
write_text:
@loop:
    lda $00,x
    beq @end
    sta IO_TX
    inx
    bra @loop
@end:
    rts

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.segment "RODATA"
txt_ok:              .byte "WAI interrupt wake OK", $0D, $0A, 0
txt_not_serviced:    .byte "FAIL: WAI woke but the IRQ was never serviced", $0D, $0A, 0
txt_wrong_pc:        .byte "FAIL: IRQ pushed the wrong return address", $0D, $0A, 0
txt_serviced_masked: .byte "FAIL: IRQ was serviced with the I flag set", $0D, $0A, 0
txt_trap:            .byte "FAIL: unexpected vector taken", $0D, $0A, 0

.segment "VECTORS"          ; $FFE0
    .word trap, trap        ; $FFE0 reserved, $FFE2 reserved
    .word trap              ; $FFE4 COP
    .word trap              ; $FFE6 BRK
    .word trap              ; $FFE8 ABORT
    .word trap              ; $FFEA NMI
    .word trap              ; $FFEC reserved
    .word irq_handler       ; $FFEE IRQ
    .word trap, trap        ; $FFF0, $FFF2 reserved
    .word trap              ; $FFF4 COP (emulation)
    .word trap              ; $FFF6 reserved
    .word trap              ; $FFF8 ABORT (emulation)
    .word trap              ; $FFFA NMI (emulation)
    .word reset             ; $FFFC RESET
    .word trap              ; $FFFE IRQ/BRK (emulation)
