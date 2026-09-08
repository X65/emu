.setcpu "65816"
.segment "CODE"
.a8
.i8
start:
    sei
    cld
    sec
    xce                         ; explicit emulation mode
    ldx #$FF
    txs
    stz $FF1A                   ; disable CGIA interrupt sources
    stz $FF1B                   ; clear pending video status
    stz $FFEC                   ; disable RIA timer IRQ gate
    ldx #0
rng_loop:
    lda $FFE2                   ; hardware RNG
    sta $0320,x
    inx
    cpx #32
    bne rng_loop
    lda #$A5
    sta $0300                   ; initialized marker
    jmp input_loop
    .res $80 - (* - start), $EA
input_loop:
    lda $FF80                   ; GPIO joystick port zero, active low
    sta $0301
    jmp input_loop
.assert input_loop - start = $80, error, "input loop must be at $2080"
