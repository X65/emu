#pragma once
/*#
    # w65c816sdasm.h

    A stateless WDC 65C816 disassembler that doesn't call any CRT functions.
    
    Do this:
    ~~~C
    #define CHIPS_UTIL_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the 
    implementation.

    Optionally provide the following macros with your own implementation
    
    ~~~C
    CHIPS_ASSERT(c)
    ~~~
        your own assert macro (default: assert(c))

    ## Usage

    There's only one function to call which consumes a stream of instruction bytes
    and produces a stream of ASCII characters for exactly one instruction:

    ~~~C
    uint16_t w65816dasm_op(uint16_t pc, uint8_t p, w65816dasm_input_t in_cb, w65816dasm_output_t out_cb, void* user_data)
    ~~~

    pc      - the current 16-bit program counter, this is used to compute 
              absolute target addresses for relative jumps
    p       - processor status register used to determine register sizes
    in_cb   - this function is called when the disassembler needs the next 
              instruction byte: uint8_t in_cb(void* user_data)
    out_cb  - (optional) this function is called when the disassembler produces a single
              ASCII character: void out_cb(char c, void* user_data)
    user_data   - a user-provided context pointer for the callbacks

    w65816dasm_op() returns the new program counter (pc), this should be
    used as input arg when calling w65816dasm_op() for the next instruction.

    NOTE that the output callback will never be called with a null character,
    you need to terminate the resulting string yourself if needed.

    Undocumented instructions are supported and are marked with a '*'.

    ## zlib/libpng license

    Copyright (c) 2018 Andre Weissflog
    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.
    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:
        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.
        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.
        3. This notice may not be removed or altered from any source
        distribution. 
#*/
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the input callback type */
typedef uint8_t (*w65816dasm_input_t)(void* user_data);
/* the output callback type */
typedef void (*w65816dasm_output_t)(char c, void* user_data);

/* disassemble a single 65816 instruction into a stream of ASCII characters */
uint16_t w65816dasm_op(uint16_t pc, uint8_t p, w65816dasm_input_t in_cb, w65816dasm_output_t out_cb, void* user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_UTIL_IMPL
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

/* fetch unsigned 8-bit value and track pc */
#ifdef _FETCH_U8
#undef _FETCH_U8
#endif
#define _FETCH_U8(v) v=in_cb(user_data);pc++;
/* fetch signed 8-bit value and track pc */
#ifdef _FETCH_I8
#undef _FETCH_I8
#endif
#define _FETCH_I8(v) v=(int8_t)in_cb(user_data);pc++;
/* fetch unsigned 16-bit value and track pc */
#ifdef _FETCH_U16
#undef _FETCH_U16
#endif
#define _FETCH_U16(v) v=in_cb(user_data);v|=in_cb(user_data)<<8;pc+=2;
/* fetch signed 16-bit value and track pc */
#ifdef _FETCH_I16
#undef _FETCH_I16
#endif
#define _FETCH_I16(v) v=(int16_t)in_cb(user_data);v|=(int16_t)(in_cb(user_data)<<8);pc+=2;
/* fetch unsigned 24-bit value and track pc */
#ifdef _FETCH_U24
#undef _FETCH_U24
#endif
#define _FETCH_U24(v) v=in_cb(user_data);v|=in_cb(user_data)<<8;v|=in_cb(user_data)<<16;pc+=3;
/* output character */
#ifdef _CHR
#undef _CHR
#endif
#define _CHR(c) if (out_cb) { out_cb(c,user_data); }
/* output string */
#ifdef _STR
#undef _STR
#endif
#define _STR(s) _w65816dasm_str(s,out_cb,user_data);
/* output number as unsigned 8-bit string (hex) */
#ifdef _STR_U8
#undef _STR_U8
#endif
#define _STR_U8(u8) _w65816dasm_u8((uint8_t)(u8),out_cb,user_data);
/* output number number as unsigned 16-bit string (hex) */
#ifdef _STR_U16
#undef _STR_U16
#endif
#define _STR_U16(u16) _w65816dasm_u16((uint16_t)(u16),out_cb,user_data);
/* output number number as unsigned 24-bit string (hex) */
#ifdef _STR_U24
#undef _STR_U24
#endif
#define _STR_U24(u32) _w65816dasm_u24((uint32_t)(u32),out_cb,user_data);

/* addressing modes */
#define A_ABS    (0)     /* a       - Absolute */
#define A_AXI    (1)     /* (a,x)   - Absolute Indexed with X Indirect */
#define A_ABX    (2)     /* a,x     - Absolute Indexed with X */
#define A_ABY    (3)     /* a,y     - Absolute Indexed with Y */
#define A_ABI    (4)     /* (a)     - Absolute Indirect */
#define A_ALX    (5)     /* al,x    - Absolute Long Indexed with X */
#define A_ALN    (6)     /* al      - Absolute Long */
#define A_ACC    (7)     /* A       - Accumulator */
#define A_BMV    (8)     /* xyc     - Block Move */
#define A_DXI    (9)     /* (d,x)   - Direct Indexed with X Indirect */
#define A_DIX    (10)    /* d,x     - Direct Indexed with X */
#define A_DIY    (11)    /* d,y     - Direct Indexed with Y */
#define A_DII    (12)    /* (d),y   - Direct Indirect Indexed with Y */
#define A_DLY    (13)    /* [d],y   - Direct Indirect Long Indexed with Y */
#define A_DIL    (14)    /* [d]     - Direct Indirect Long */
#define A_DID    (15)    /* (d)     - Direct Indirect */
#define A_DIR    (16)    /* d       - Direct */
#define A_IMM    (17)    /* #       - Immediate */
#define A_IMP    (18)    /* i       - Implied */
#define A_PCL    (19)    /* rl      - Program Counter Relative Long */
#define A_PCR    (20)    /* r       - Program Counter Relative */
#define A_STC    (21)    /* s       - Stack */
#define A_STR    (22)    /* d,s     - Stack Relative */
#define A_SII    (23)    /* (d,s),y - Stack Relative Indirect Indexed with Y */
#define A_STS    (24)    /* s       - Stack with Signature */
#define A_JMP    (25)    /* special JMP abs */
#define A_JSR    (26)    /* special JSR abs */
#define A_INV    (28)    /* this is an invalid instruction */

/* opcode descriptions */
static uint8_t _w65816dasm_ops[4][8][8] = {
/* cc = 00 */
{
    //---  BIT   JMP   JMP() STY   LDY   CPY   CPX
    {A_STS,A_JSR,A_STC,A_STC,A_PCR,A_IMM,A_IMM,A_IMM},
    {A_DIR,A_DIR,A_BMV,A_DIR,A_DIR,A_DIR,A_DIR,A_DIR},
    {A_STC,A_STC,A_STC,A_STC,A_IMP,A_IMP,A_IMP,A_IMP},
    {A_ABS,A_ABS,A_JMP,A_ABI,A_ABS,A_ABS,A_ABS,A_ABS},
    {A_PCR,A_PCR,A_PCR,A_PCR,A_PCR,A_PCR,A_PCR,A_PCR},
    {A_DIR,A_DIX,A_BMV,A_DIX,A_DIX,A_DIX,A_DID,A_ABS},
    {A_IMP,A_IMP,A_IMP,A_IMP,A_IMP,A_IMP,A_IMP,A_IMP},
    {A_ABS,A_ABX,A_ALN,A_AXI,A_ABS,A_ABX,A_ABI,A_AXI},
},
/* cc = 01 */
{
    //ORA  AND   EOR   ADC   STA   LDA   CMP   SBC
    {A_DXI,A_DXI,A_DXI,A_DXI,A_DXI,A_DXI,A_DXI,A_DXI},
    {A_DIR,A_DIR,A_DIR,A_DIR,A_DIR,A_DIR,A_DIR,A_DIR},
    {A_IMM,A_IMM,A_IMM,A_IMM,A_IMM,A_IMM,A_IMM,A_IMM},
    {A_ABS,A_ABS,A_ABS,A_ABS,A_ABS,A_ABS,A_ABS,A_ABS},
    {A_DII,A_DII,A_DII,A_DII,A_DII,A_DII,A_DII,A_DII},
    {A_DIX,A_DIX,A_DIX,A_DIX,A_DIX,A_DIX,A_DIX,A_DIX},
    {A_ABY,A_ABY,A_ABY,A_ABY,A_ABY,A_ABY,A_ABY,A_ABY},
    {A_ABX,A_ABX,A_ABX,A_ABX,A_ABX,A_ABX,A_ABX,A_ABX},
},
/* cc = 02 */
{
    //ASL  ROL   LSR   ROR   STX   LDX   DEC   INC
    {A_STS,A_ALN,A_STS,A_PCL,A_PCL,A_IMM,A_IMM,A_IMM},
    {A_DIR,A_DIR,A_DIR,A_DIR,A_DIR,A_DIR,A_DIR,A_DIR},
    {A_ACC,A_ACC,A_ACC,A_ACC,A_IMP,A_IMP,A_IMP,A_IMP},
    {A_ABS,A_ABS,A_ABS,A_ABS,A_ABS,A_ABS,A_ABS,A_ABS},
    {A_DID,A_DID,A_DID,A_DID,A_DID,A_DID,A_DID,A_DID},
    {A_DIX,A_DIX,A_DIX,A_DIX,A_DIY,A_DIY,A_DIX,A_DIX},
    {A_ACC,A_ACC,A_STC,A_STC,A_IMP,A_IMP,A_STC,A_STC},
    {A_ABX,A_ABX,A_ABX,A_ABX,A_ABX,A_ABY,A_ABX,A_ABX},
},
/* cc = 03 */
{
    {A_STR,A_STR,A_STR,A_STR,A_STR,A_STR,A_STR,A_STR},
    {A_DIL,A_DIL,A_DIL,A_DIL,A_DIL,A_DIL,A_DIL,A_DIL},
    {A_STC,A_STC,A_STC,A_STC,A_STC,A_STC,A_IMP,A_IMP},
    {A_ALN,A_ALN,A_ALN,A_ALN,A_ALN,A_ALN,A_ALN,A_ALN},
    {A_SII,A_SII,A_SII,A_SII,A_SII,A_SII,A_SII,A_SII},
    {A_DLY,A_DLY,A_DLY,A_DLY,A_DLY,A_DLY,A_DLY,A_DLY},
    {A_IMP,A_IMP,A_IMP,A_IMP,A_IMP,A_IMP,A_IMP,A_IMP},
    {A_ALX,A_ALX,A_ALX,A_ALX,A_ALX,A_ALX,A_ALX,A_ALX},
} };

static const char* _w65816dasm_hex = "0123456789ABCDEF";

/* helper function to output string */
static void _w65816dasm_str(const char* str, w65816dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        char c;
        while (0 != (c = *str++)) {
            out_cb(c, user_data);
        }
    }
}

/* helper function to output an unsigned 8-bit value as hex string */
static void _w65816dasm_u8(uint8_t val, w65816dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        out_cb('$', user_data);
        for (int i = 1; i >= 0; i--) {
            out_cb(_w65816dasm_hex[(val>>(i*4)) & 0xF], user_data);
        }
    }
}

/* helper function to output an unsigned 16-bit value as hex string */
static void _w65816dasm_u16(uint16_t val, w65816dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        out_cb('$', user_data);
        for (int i = 3; i >= 0; i--) {
            out_cb(_w65816dasm_hex[(val>>(i*4)) & 0xF], user_data);
        }
    }
}

/* helper function to output an unsigned 24-bit value as hex string */
static void _w65816dasm_u24(uint32_t val, w65816dasm_output_t out_cb, void* user_data) {
    if (out_cb) {
        out_cb('$', user_data);
        for (int i = 5; i >= 0; i--) {
            out_cb(_w65816dasm_hex[(val>>(i*4)) & 0xF], user_data);
        }
    }
}

/* main disassembler function */
uint16_t w65816dasm_op(uint16_t pc, uint8_t p, w65816dasm_input_t in_cb, w65816dasm_output_t out_cb, void* user_data) {
    CHIPS_ASSERT(in_cb);
    uint8_t op;
    _FETCH_U8(op);
    uint8_t cc  = op & 0x03;
    uint8_t bbb = (op >> 2) & 0x07;
    uint8_t aaa = (op >> 5) & 0x07;

    /* opcode name */
    const char* n = "???";
    bool indirect = false;
    bool ignore_signature = false;
    bool x8 = true;
    switch (cc) {
        case 0:
            switch (aaa) {
                case 0:
                    switch (bbb) {
                        case 0:  n = "BRK"; ignore_signature = true; break;
                        case 1:  n = "TSB"; break;
                        case 2:  n = "PHP"; break;
                        case 3:  n = "TSB"; break;
                        case 4:  n = "BPL"; break;
                        case 5:  n = "TRB"; break;
                        case 6:  n = "CLC"; break;
                        case 7:  n = "TRB"; break;
                        default: n = "*NOP"; break;
                    }
                    break;
                case 1:
                    switch (bbb) {
                        case 0:  n = "JSR"; break;
                        case 2:  n = "PLP"; break;
                        case 4:  n = "BMI"; break;
                        case 6:  n = "SEC"; break;
                        default: n = "BIT"; x8 = p & 0x20; break;
                    }
                    break;
                case 2:
                    switch (bbb) {
                        case 0:  n = "RTI"; break;
                        case 1:  n = "MVP"; break;
                        case 2:  n = "PHA"; break;
                        case 3:  n = "JMP"; break;
                        case 4:  n = "BVC"; break;
                        case 5:  n = "MVN"; break;
                        case 6:  n = "CLI"; break;
                        case 7:  n = "JMP"; break;
                        default: n = "*NOP"; break;
                    }
                    break;
                case 3:
                    switch (bbb) {
                        case 0:  n = "RTS"; break;
                        case 2:  n = "PLA"; break;
                        case 3:  n = "JMP"; indirect = true; break;  /* jmp (a) */
                        case 4:  n = "BVS"; break;
                        case 6:  n = "SEI"; break;
                        case 7:  n = "JMP"; indirect = true; break;  /* jmp (a,x) */
                        default: n = "STZ"; break;
                    }
                    break;
                case 4:
                    switch (bbb) {
                        case 0:  n = "BRA"; break;
                        case 2:  n = "DEY"; break;
                        case 4:  n = "BCC"; break;
                        case 6:  n = "TYA"; break;
                        case 7:  n = "STZ"; break;
                        default: n = "STY"; break;
                    }
                    break;
                case 5:
                    switch (bbb) {
                        case 2:  n = "TAY"; break;
                        case 4:  n = "BCS"; break;
                        case 6:  n = "CLV"; break;
                        default: n = "LDY"; x8 = p & 0x10; break;
                    }
                    break;
                case 6:
                    switch (bbb) {
                        case 2:  n = "INY"; break;
                        case 4:  n = "BNE"; break;
                        case 5:  n = "PEI"; break;
                        case 6:  n = "CLD"; break;
                        case 7:  n = "JML"; break;
                        default: n = "CPY"; x8 = p & 0x10; break;
                    }
                    break;
                case 7:
                    switch (bbb) {
                        case 2:  n = "INX"; break;
                        case 4:  n = "BEQ"; break;
                        case 5:  n = "PEA"; break;
                        case 6:  n = "SED"; break;
                        case 7:  n = "JSR"; indirect = true; break;  /* jsr (a,x) */
                        default: n = "CPX"; x8 = p & 0x10; break;
                    }
                    break;
            }
            break;

        case 1:
            switch (aaa) {
                case 0: n = "ORA"; x8 = p & 0x20; break;
                case 1: n = "AND"; x8 = p & 0x20; break;
                case 2: n = "EOR"; x8 = p & 0x20; break;
                case 3: n = "ADC"; x8 = p & 0x20; break;
                case 4:
                    switch (bbb) {
                        case 2:  n = "BIT"; x8 = p & 0x20; break;
                        default: n = "STA"; break;
                    }
                    break;
                case 5: n = "LDA"; x8 = p & 0x20; break;
                case 6: n = "CMP"; x8 = p & 0x20; break;
                case 7: n = "SBC"; x8 = p & 0x20; break;
            }
            break;

        case 2:
            switch (aaa) {
                case 0:
                    switch (bbb) {
                        case 0:  n = "COP"; break;
                        case 2:  n = "ASL"; break; /* ASL A */
                        case 4:  n = "ORA"; x8 = p & 0x20; break;
                        case 6:  n = "INC"; break; /* INC A */
                        default: n = "ASL"; break;
                    }
                    break;
                case 1:
                    switch (bbb) {
                        case 0:  n = "JSL"; break;
                        case 2:  n = "ROL"; break; /* ROL A */
                        case 4:  n = "AND"; x8 = p & 0x20; break;
                        case 6:  n = "DEC"; break; /* DEC A */
                        default: n = "ROL"; break;
                    }
                    break;
                case 2:
                    switch (bbb) {
                        case 0:  n = "WDM"; break;
                        case 2:  n = "LSR"; break; /* LSR A */
                        case 4:  n = "EOR"; x8 = p & 0x20; break;
                        case 6:  n = "PHY"; break;
                        default: n = "LSR"; break;
                    }
                    break;
                case 3:
                    switch (bbb) {
                        case 0:  n = "PER"; break;
                        case 2:  n = "ROR"; break; /* ROR A */
                        case 4:  n = "ADC"; x8 = p & 0x20; break;
                        case 6:  n = "PLY"; break;
                        default: n = "ROR"; break;
                    }
                    break;
                case 4:
                    switch (bbb) {
                        case 0:  n = "BRL"; break;
                        case 2:  n = "TXA"; break;
                        case 4:  n = "STA"; break;
                        case 6:  n = "TXS"; break;
                        case 7:  n = "STZ"; break;
                        default: n = "STX"; break;
                    }
                    break;
                case 5:
                    switch (bbb) {
                        case 2:  n = "TAX"; break;
                        case 4:  n = "LDA"; x8 = p & 0x20; break;
                        case 6:  n = "TSX"; break;
                        default: n = "LDX"; x8 = p & 0x10; break;
                    }
                    break;
                case 6:
                    switch (bbb) {
                        case 0:  n = "REP"; break;
                        case 2:  n = "DEX"; break;
                        case 4:  n = "CMP"; x8 = p & 0x20; break;
                        case 6:  n = "PHX"; break;
                        default: n = "DEC"; break;
                    }
                    break;
                case 7:
                    switch (bbb) {
                        case 0:  n = "SEP"; break;
                        case 2:  n = "NOP"; break;
                        case 4:  n = "SBC"; x8 = p & 0x20; break;
                        case 6:  n = "PLX"; break;
                        default: n = "INC"; break;
                    }
                    break;
            }
            break;

        case 3:
            switch (aaa) {
                case 0:
                    switch (bbb) {
                        case 2:  n = "PHD"; break;
                        case 6:  n = "TCS"; break;
                        default: n = "ORA"; x8 = p & 0x20; break;
                    }
                    break;
                case 1:
                    switch (bbb) {
                        case 2:  n = "PLD"; break;
                        case 6:  n = "TSC"; break;
                        default: n = "AND"; x8 = p & 0x20; break;
                    }
                    break;
                case 2:
                    switch (bbb) {
                        case 2:  n = "PHK"; break;
                        case 6:  n = "TCD"; break;
                        default: n = "EOR"; x8 = p & 0x20; break;
                    }
                    break;
                case 3:
                    switch (bbb) {
                        case 2:  n = "RTL"; break;
                        case 6:  n = "TDC"; break;
                        default: n = "ADC"; x8 = p & 0x20; break;
                    }
                    break;
                case 4:
                    switch (bbb) {
                        case 2:  n = "PHB"; break;
                        case 6:  n = "TXY"; break;
                        default: n = "STA"; break;
                    }
                    break;
                case 5:
                    switch (bbb) {
                        case 2:  n = "PLB"; break;
                        case 6:  n = "TYX"; break;
                        default: n = "LDA"; x8 = p & 0x20; break;
                    }
                    break;
                case 6:
                    switch (bbb) {
                        case 2:  n = "WAI"; break;
                        case 6:  n = "STP"; break;
                        default: n = "CMP"; x8 = p & 0x20; break;
                    }
                    break;
                case 7:
                    switch (bbb) {
                        case 2:  n = "XBA"; break;
                        case 6:  n = "XCE"; break;
                        default: n = "SBC"; x8 = p & 0x20; break;
                    }
                    break;
            }
    }
    _STR(n);

    uint8_t u8; int8_t i8; uint16_t u16; int16_t i16; uint32_t u24;
    switch (_w65816dasm_ops[cc][bbb][aaa]) {
        case A_IMP: /* i       - Implied */
        case A_STC: /* s       - Stack */
            break;
        case A_ACC: /* A       - Accumulator */
            _STR(" A");
            break;
        case A_STS: /* s       - Stack with Signature */
            if (!ignore_signature)
            {_CHR(' '); _FETCH_U8(u8); _STR_U8(u8);}
            break;
        case A_IMM: /* #       - Immediate */
            if (x8) {_CHR(' '); _FETCH_U8(u8); _CHR('#'); _STR_U8(u8);}
            else    {_CHR(' '); _FETCH_U16(u16); _CHR('#'); _STR_U16(u16);}
            break;
        case A_DIR: /* d       - Direct */
            _CHR(' '); _FETCH_U8(u8); _STR_U8(u8);
            break;
        case A_DIX: /* d,x     - Direct Indexed with X */
            _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _STR(",X");
            break;
        case A_DIY: /* d,y     - Direct Indexed with Y */
            _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _STR(",Y");
            break;
        case A_ABS: /* a       - Absolute */
        case A_JSR: /* special JSR abs */
        case A_JMP: /* special JMP abs */
            _CHR(' '); _FETCH_U16(u16);
            if (indirect) {_CHR('('); _STR_U16(u16); _CHR(')');}
            else          {_STR_U16(u16);}
            break;
        case A_ABX: /* a,x     - Absolute Indexed with X */
            _CHR(' '); _FETCH_U16(u16); _STR_U16(u16); _STR(",X");
            break;
        case A_ABY: /* a,y     - Absolute Indexed with Y */
            _CHR(' '); _FETCH_U16(u16); _STR_U16(u16); _STR(",Y");
            break;
        case A_ABI: /* (a)     - Absolute Indirect */
            _CHR(' '); _FETCH_U16(u16); _CHR('('); _STR_U16(u16); _STR(")");
            break;
        case A_AXI: /* (a,x)   - Absolute Indexed with X Indirect */
            _CHR(' '); _FETCH_U16(u16); _CHR('('); _STR_U16(u16); _STR(",X)");
            break;
        case A_DID: /* (d)     - Direct Indirect */
            _CHR(' '); _FETCH_U8(u8); _CHR('('); _STR_U8(u8); _STR(")");
            break;
        case A_DXI: /* (d,x)   - Direct Indexed with X Indirect */
            _CHR(' '); _FETCH_U8(u8); _CHR('('); _STR_U8(u8); _STR(",X)");
            break;
        case A_DII: /* (d),y   - Direct Indirect Indexed with Y */
            _CHR(' '); _FETCH_U8(u8); _CHR('('); _STR_U8(u8); _STR("),Y");
            break;
        case A_PCR: /* r       - Program Counter Relative */
            /* compute target address */
            _CHR(' '); _FETCH_I8(i8); _STR_U16(pc+i8);
            break;
        case A_PCL: /* rl      - Program Counter Relative Long */
            /* compute target address */
            _CHR(' '); _FETCH_I16(i16); _STR_U16(pc+i16);
            break;
        case A_STR: /* d,s     - Stack Relative */
            _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _STR(",S");
            break;
        case A_SII: /* (d,s),y - Stack Relative Indirect Indexed with Y */
            _CHR(' '); _FETCH_U8(u8); _CHR('('); _STR_U8(u8); _STR(",S),Y");
            break;
        case A_BMV: /* xyc     - Block Move */
            _CHR(' '); _FETCH_U8(u8); _STR_U8(u8); _FETCH_U8(u8); _STR(", "); _STR_U8(u8);
            break;
        case A_ALX: /* al,x    - Absolute Long Indexed with X */
            _CHR(' '); _FETCH_U24(u24); _STR_U24(u24); _STR(",X");
            break;
        case A_ALN: /* al      - Absolute Long */
            _CHR(' '); _FETCH_U24(u24); _STR_U24(u24);
            break;
        case A_DIL: /* [d]     - Direct Indirect Long */
            _CHR(' '); _FETCH_U8(u8); _CHR('['); _STR_U8(u8); _STR("]");
            break;
        case A_DLY: /* [d],y   - Direct Indirect Long Indexed with Y */
            _CHR(' '); _FETCH_U8(u8); _CHR('['); _STR_U8(u8); _STR("],Y");
            break;

    }
    return pc;
}

#undef _FETCH_I8
#undef _FETCH_U8
#undef _FETCH_U16
#undef _CHR
#undef _STR
#undef _STR_U8
#undef _STR_U16
#undef A____
#undef A_IMM
#undef A_ZER
#undef A_ZPX
#undef A_ZPY
#undef A_ABS
#undef A_ABX
#undef A_ABY
#undef A_IDX
#undef A_IDY
#undef A_JMP
#undef A_JSR
#undef A_INV
#undef M___
#undef M_R_
#undef M__W
#undef M_RW
#endif /* CHIPS_UTIL_IMPL */
