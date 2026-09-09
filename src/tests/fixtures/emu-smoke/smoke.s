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
    lda $FF80                   ; DE-9 joystick port 1, active low
    sta $0301
    lda $FF81                   ; DE-9 joystick port 2
    sta $0306
    ; USB HID gamepads 1 and 2, active high.  The selector is
    ; (index << 4) | device; device $02 is a gamepad.  Bit 7 of the first
    ; report byte says a pad is connected.
    lda #$12
    sta $FFB0
    lda $FFB0                   ; pad 1 dpad + flags
    sta $0302
    lda $FFB2                   ; pad 1 button0
    sta $0303
    lda #$22
    sta $FFB0
    lda $FFB0                   ; pad 2 dpad + flags
    sta $0304
    lda $FFB2                   ; pad 2 button0
    sta $0305
    ; Keyboard: a 256-bit map of held keys, sixteen bytes to a page.  Byte 3
    ; of page 0 carries W ($1A), byte 12 of page 1 carries left shift ($E1).
    lda #$00
    sta $FFB0
    lda $FFB3
    sta $0307
    lda #$10
    sta $FFB0
    lda $FFBC
    sta $0308
    jmp input_loop
.assert input_loop - start = $80, error, "input loop must be at $2080"
