; Character output for the cpuemu build.
;
; cpuemu's --output HEX makes writes to one bank 0 address appear on stdout and
; never reach memory, which is the whole device: no status register, no reset,
; and nothing to poll. $7F01 is the address Daryl used for the Kowalski port
; and is clear of both the ROM image and the areas the tests scratch.
;
; There is deliberately no character input. The suite only ever needed it for
; an interactive "press a key" pause, and asking cpuemu for an input port costs
; a read(2) on every CPU tick.

IO_TX = $7F01

.segment "CODE"

; print the character in A, preserving every register
.a8
.i16
.proc chrout
    sta f:IO_TX     ; long, so the data bank does not matter
    rts
.endproc
