; Harness for gilyon's 65816 test suite, targeting the cpuemu simulator.
;
; Adapted from the upstream SNES main.asm (github.com/gilyon/snes-tests, MIT).
; The test bodies in tests-full.inc and tests_table.inc are used unchanged;
; only the harness around them differs. Following Daryl's Kowalski port
; (6502.org forum topic 8119), the SNES pieces are gone - no PPU register
; setup, no VRAM, no vblank throttling, no joypad - and text goes to a
; character port instead. See README.md.
;
; Two departures from that port, both so this can run unattended in CI:
;   - no input at all, so the run never blocks on a port that will not answer
;   - the suite halts with STP instead of looping, and reports a verdict, so
;     cpuemu terminates on its own
;
; The generated tests call into init_test, save_results, bankN_save_results,
; fail and success; those keep their upstream signatures.

.p816
.i16
.a8

.include "io.asm"

native_brk_handler = $1000
native_cop_handler = $1004
emulation_brk_handler = $1008
emulation_cop_handler = $100C

; cpuemu --dump reads this byte after the run: FF once every test has passed.
; Lives in the IO page, which the tests leave alone - they scratch bank 0 pages
; 0-3 and banks 6 and 7.
RESULT = $7F00
RESULT_OK = $FF

.segment "VECTORS"
    .word 0, 0, native_cop_handler, native_brk_handler, 0, 0, 0, 0
    .word 0, 0, emulation_cop_handler, 0, 0, 0, main, emulation_brk_handler

.segment "ZEROPAGE"
.res $10
test_num: .word 0
result_a: .word 0
result_x: .word 0
result_y: .word 0
result_p: .word 0
result_s: .word 0
result_d: .word 0
result_dbr: .byte 0
retaddr: .word 0  ; return address from bankN_save_results
fail_count: .word 0

.segment "CODE"

main:
    clc
    xce
    sei
    rep #$18  ; 16 bit X/Y
    sep #$20  ; 8 bit A
    ldx #$01EF
    txs

    lda #$00
    pha
    plb       ; data bank 0

    jsr init_mem

    ldx #txt_running
    jsr write_text

    ldx #$0000
    stx fail_count
    ldx #$ffff
    stx test_num
    jmp start_tests

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

init_mem:
    ; Put STP opcodes where an errant jump or an unexpected software interrupt
    ; would land, so the run stops instead of wandering off.
    lda #$DB
    sta f:native_brk_handler
    sta f:native_cop_handler
    sta f:emulation_brk_handler
    sta f:emulation_cop_handler
    lda #$00
    sta f:RESULT
    rts

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 0:x = text (null-terminated)
write_text:
@loop:
    lda $00,x
    beq @end
    jsr chrout
    inx
    bra @loop
@end:
    rts

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; a = val
write_hex8:
    pha
    lsr a
    lsr a
    lsr a
    lsr a
    clc
    jsr @write_digit
    pla
    and #$0F
@write_digit:  ; write hex digit in A
    cmp #$0A
    bcc @num
    clc
    adc #'A'-$0A-'0'
@num:
    clc
    adc #'0'
    jmp chrout

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 0:x = address of a 16-bit value
write_hex16:
    lda $01,x
    jsr write_hex8
    lda $00,x
    jmp write_hex8

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

newline:
    lda #$0D
    jsr chrout
    lda #$0A
    jmp chrout

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; x = new test num
init_test:
    ; Check that we haven't skipped a test
    dex
    cpx test_num
    beq @ok

    ; ** Invalid test order - possibly an errant jump **
    clc
    xce
    sei
    rep #$18  ; 16 bit X/Y
    sep #$20  ; 8 bit A
    ldx #$01EF
    txs
    lda #$00
    pha
    plb

    ldx #txt_skipped
    jsr write_text
    jmp halt_fail

@ok:
    inx
    stx test_num
    rtl

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Save the register values, and reset state (D, DBR, etc.)
save_results:
    ; p register was already saved, and emulation mode was cleared.
    sei
    rep #$38
    .a16
    .i16
    phd
    pha
    lda #$0000
    tcd
    pla
    sta result_a
    stx result_x
    sty result_y
    plx  ; d register
    stx result_d
    tsc  ; original S value minus 3 (due to jsl).
    inc a
    inc a
    inc a
    sta result_s

    sep #$20
    .a8
    phb
    pla
    sta result_dbr
    lda #$00
    pha
    plb
    rtl

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Reached after the last test in tests_table.
success:
    rep #$20
    .a16
    lda fail_count
    sep #$20
    .a8
    bne @failed

    ldx #txt_success
    jsr write_text
    jsr newline
    lda #RESULT_OK
    sta f:RESULT
    stp

@failed:
    ldx #txt_failures
    jsr write_text
    ldx #fail_count
    jsr write_hex16
    jsr newline
    jmp halt_fail

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

halt_fail:
    lda #$00
    sta f:RESULT
    stp

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; A test jumps here when its result does not match. Report it and carry on to
; the next test, so one run shows every failure rather than only the first.
fail:
    ldx #$1ef
    txs  ; in case s is invalid

    rep #$20
    .a16
    inc fail_count
    sep #$20
    .a8

    ldx #txt_test
    jsr write_text
    ldx #test_num
    jsr write_hex16
    ldx #txt_fail
    jsr write_text

    ldx #txt_a
    jsr write_text
    ldx #result_a
    jsr write_hex16
    ldx #txt_x
    jsr write_text
    ldx #result_x
    jsr write_hex16
    ldx #txt_y
    jsr write_text
    ldx #result_y
    jsr write_hex16
    ldx #txt_p
    jsr write_text
    lda result_p
    jsr write_hex8
    ldx #txt_s
    jsr write_text
    ldx #result_s
    jsr write_hex16
    jsr newline

    ; jump to the next test
    rep #$20
    .a16
    lda test_num
    inc a
    asl a  ; A = (test_num+1) * 2
    sec
    adc test_num  ; A = (test_num+1) * 3
    tax
    sep #$20
    .a8
    ldy tests_table,x   ; y = test offset
    lda tests_table+2,x ; a = test bank
    pha
    dey  ; the return address should be 1 less than the target
    phy
    rtl  ; actually a jump to the next test

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

.include "tests-full.inc"

.segment "RODATA"
txt_running:  .byte "Running 65816 tests...", $0D, $0A, 0
txt_success:  .byte "Success", 0
txt_test:     .byte "Test ", 0
txt_fail:     .byte " Failed  ", 0
txt_failures: .byte "Failed tests: $", 0
txt_skipped:  .byte "Invalid test order", $0D, $0A, 0
txt_a: .byte "A=", 0
txt_x: .byte " X=", 0
txt_y: .byte " Y=", 0
txt_p: .byte " P=", 0
txt_s: .byte " S=", 0

tests_table:
    .include "tests_table.inc"
    .faraddr success

.segment "TEST_DATA"  ; At address FFA0. Used by some tests
test_addr:    ; $FFA0
    .word $1212
test_target:  ; $FFA2
    .word $8000
test_target24:  ; $FFA4
    .word $8000
    .byte $7E
