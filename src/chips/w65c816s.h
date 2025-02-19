#pragma once
/*#
    # w65c816s.h

    Western Design Center 65C816 CPU emulator.

    Project repo: https://github.com/floooh/chips/

    NOTE: this file is code-generated from w65816.template.h and w65816_gen.py
    in the 'codegen' directory.

    Do this:
    ~~~C
    #define CHIPS_IMPL
    ~~~
    before you include this file in *one* C or C++ file to create the
    implementation.

    Optionally provide the following macros with your own implementation
    ~~~C
    CHIPS_ASSERT(c)
    ~~~

    ## Emulated Pins

    ***********************************
    *           +-----------+         *
    *   IRQ --->|           |---> A0  *
    *   NMI --->|           |...      *
    *    RDY--->|           |---> A23 *
    *    RES--->|           |         *
    *    RW <---|           |         *
    *  SYNC <---|           |         *
    *           |           |<--> D0  *
    *           |           |...      *
    *           |           |<--> D7  *
    *           |           |         *
    *           +-----------+         *
    ***********************************

    If the RDY pin is active (1) the CPU will loop on the next read
    access until the pin goes inactive.

    ## Overview

    w65c816s.h implements a cycle-stepped 65816 CPU emulator, meaning
    that the emulation state can be ticked forward in clock cycles instead
    of full instructions.

    To initialize the emulator, fill out a w65816_desc_t structure with
    initialization parameters and then call w65816_init().

        ~~~C
        typedef struct {
            bool bcd_disabled;          // set to true if BCD mode is disabled
         } w65816_desc_t;
         ~~~

    At the end of w65816_init(), the CPU emulation will be at the start of
    RESET state, and the first 7 ticks will execute the reset sequence
    (loading the reset vector at address 0xFFFC and continuing execution
    there.

    w65816_init() will return a 64-bit pin mask which must be the input argument
    to the first call of w65816_tick().

    To execute instructions, call w65816_tick() in a loop. w65816_tick() takes
    a 64-bit pin mask as input, executes one clock tick, and returns
    a modified pin mask.

    After executing one tick, the pin mask must be inspected, a memory read
    or write operation must be performed, and the modified pin mask must be
    used for the next call to w65816_tick(). This 64-bit pin mask is how
    the CPU emulation communicates with the outside world.

    The simplest-possible execution loop would look like this:

        ~~~C
        // setup 16 MBytes of memory
        uint8_t mem[1<<24] = { ... };
        // initialize the CPU
        w65816_t cpu;
        uint64_t pins = w65816_init(&cpu, &(w65816_desc_t){...});
        while (...) {
            // run the CPU emulation for one tick
            pins = w65816_tick(&cpu, pins);
            // extract 24-bit address from pin mask
            const uint32_t addr = W65816_GET_ADDR(pins);
            // perform memory access
            if (pins & W65816_RW) {
                // a memory read
                W65816_SET_DATA(pins, mem[addr]);
            }
            else {
                // a memory write
                mem[addr] = W65816_GET_DATA(pins);
            }
        }
        ~~~

    To start a reset sequence, set the W65816_RES bit in the pin mask and
    continue calling the w65816_tick() function. At the start of the next
    instruction, the CPU will initiate the 7-tick reset sequence. You do NOT
    need to clear the W65816_RES bit, this will be cleared when the reset
    sequence starts.

    To request an interrupt, set the W65816_IRQ or W65816_NMI bits in the pin
    mask and continue calling the tick function. The interrupt sequence
    will be initiated at the end of the current or next instruction
    (depending on the exact cycle the interrupt pin has been set).

    Unlike the W65816_RES pin, you are also responsible for clearing the
    interrupt pins (typically, the interrupt lines are cleared by the chip
    which requested the interrupt once the CPU reads a chip's interrupt
    status register to check which chip requested the interrupt).

    To find out whether a new instruction is about to start, check if the
    both W65816_VPA and W65816_VDA pins are set.

    To "goto" a random address at any time, a 'prefetch' like this is
    necessary (this basically simulates a normal instruction fetch from
    address 'next_pc'). This is usually only needed in "trap code" which
    intercepts operating system calls, executes some native code to emulate
    the operating system call, and then continue execution somewhere else:

        ~~~C
        pins = W65816_VPA|W65816_VDA;
        W65816_SET_ADDR(pins, next_pc);
        W65816_SET_DATA(pins, mem[next_pc]);
        w65816_set_pc(next_pc);
        ~~~~

    ## Functions
    ~~~C
    uint64_t w65816_init(w65816_t* cpu, const w65816_desc_t* desc)
    ~~~
        Initialize a w65816_t instance, the desc structure provides
        initialization attributes:
            ~~~C
            typedef struct {
                bool bcd_disabled;              // set to true if BCD mode is disabled
            } w65816_desc_t;
            ~~~

    ~~~C
    uint64_t w65816_tick(w65816_t* cpu, uint64_t pins)
    ~~~
        Tick the CPU for one clock cycle. The 'pins' argument and return value
        is the current state of the CPU pins used to communicate with the
        outside world (see the Overview section above for details).

    ~~~C
    void w65816_set_x(w65816_t* cpu, uint8_t val)
    void w65816_set_xx(w65816_t* cpu, uint16_t val)
    uint8_t w65816_x(w65816_t* cpu)
    uint16_t w65816_xx(w65816_t* cpu)
    ~~~
        Set and get 658165 registers and flags.


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

// address bus pins
#define W65816_PIN_A0    (0)
#define W65816_PIN_A1    (1)
#define W65816_PIN_A2    (2)
#define W65816_PIN_A3    (3)
#define W65816_PIN_A4    (4)
#define W65816_PIN_A5    (5)
#define W65816_PIN_A6    (6)
#define W65816_PIN_A7    (7)
#define W65816_PIN_A8    (8)
#define W65816_PIN_A9    (9)
#define W65816_PIN_A10   (10)
#define W65816_PIN_A11   (11)
#define W65816_PIN_A12   (12)
#define W65816_PIN_A13   (13)
#define W65816_PIN_A14   (14)
#define W65816_PIN_A15   (15)

// data bus pins
#define W65816_PIN_D0    (16)
#define W65816_PIN_D1    (17)
#define W65816_PIN_D2    (18)
#define W65816_PIN_D3    (19)
#define W65816_PIN_D4    (20)
#define W65816_PIN_D5    (21)
#define W65816_PIN_D6    (22)
#define W65816_PIN_D7    (23)

// control pins
#define W65816_PIN_RW    (24)      // out: memory read or write access
#define W65816_PIN_VPA   (25)      // out: valid program address
#define W65816_PIN_VDA   (26)      // out: valid data address
#define W65816_PIN_IRQ   (27)      // in: maskable interrupt requested
#define W65816_PIN_NMI   (28)      // in: non-maskable interrupt requested
#define W65816_PIN_RDY   (29)      // in: freeze execution at next read cycle
#define W65816_PIN_RES   (30)      // in: request RESET
#define W65816_PIN_ABORT (31)      // in: request ABORT (not implemented)

// bank address pins
#define W65816_PIN_A16   (32)
#define W65816_PIN_A17   (33)
#define W65816_PIN_A18   (34)
#define W65816_PIN_A19   (35)
#define W65816_PIN_A20   (36)
#define W65816_PIN_A21   (37)
#define W65816_PIN_A22   (38)
#define W65816_PIN_A23   (39)

// pin bit masks
#define W65816_A0    (1ULL<<W65816_PIN_A0)
#define W65816_A1    (1ULL<<W65816_PIN_A1)
#define W65816_A2    (1ULL<<W65816_PIN_A2)
#define W65816_A3    (1ULL<<W65816_PIN_A3)
#define W65816_A4    (1ULL<<W65816_PIN_A4)
#define W65816_A5    (1ULL<<W65816_PIN_A5)
#define W65816_A6    (1ULL<<W65816_PIN_A6)
#define W65816_A7    (1ULL<<W65816_PIN_A7)
#define W65816_A8    (1ULL<<W65816_PIN_A8)
#define W65816_A9    (1ULL<<W65816_PIN_A9)
#define W65816_A10   (1ULL<<W65816_PIN_A10)
#define W65816_A11   (1ULL<<W65816_PIN_A11)
#define W65816_A12   (1ULL<<W65816_PIN_A12)
#define W65816_A13   (1ULL<<W65816_PIN_A13)
#define W65816_A14   (1ULL<<W65816_PIN_A14)
#define W65816_A15   (1ULL<<W65816_PIN_A15)
#define W65816_A16   (1ULL<<W65816_PIN_A16)
#define W65816_A17   (1ULL<<W65816_PIN_A17)
#define W65816_A18   (1ULL<<W65816_PIN_A18)
#define W65816_A19   (1ULL<<W65816_PIN_A19)
#define W65816_A20   (1ULL<<W65816_PIN_A20)
#define W65816_A21   (1ULL<<W65816_PIN_A21)
#define W65816_A22   (1ULL<<W65816_PIN_A22)
#define W65816_A23   (1ULL<<W65816_PIN_A23)
#define W65816_D0    (1ULL<<W65816_PIN_D0)
#define W65816_D1    (1ULL<<W65816_PIN_D1)
#define W65816_D2    (1ULL<<W65816_PIN_D2)
#define W65816_D3    (1ULL<<W65816_PIN_D3)
#define W65816_D4    (1ULL<<W65816_PIN_D4)
#define W65816_D5    (1ULL<<W65816_PIN_D5)
#define W65816_D6    (1ULL<<W65816_PIN_D6)
#define W65816_D7    (1ULL<<W65816_PIN_D7)
#define W65816_RW    (1ULL<<W65816_PIN_RW)
#define W65816_VPA   (1ULL<<W65816_PIN_VPA)
#define W65816_VDA   (1ULL<<W65816_PIN_VDA)
#define W65816_IRQ   (1ULL<<W65816_PIN_IRQ)
#define W65816_NMI   (1ULL<<W65816_PIN_NMI)
#define W65816_RDY   (1ULL<<W65816_PIN_RDY)
#define W65816_RES   (1ULL<<W65816_PIN_RES)
#define W65816_ABORT (1ULL<<W65816_PIN_ABORT)

/* bit mask for all CPU pins (up to bit pos 40) */
#define W65816_PIN_MASK ((1ULL<<40)-1)

/* status indicator flags */
#define W65816_EF    (1<<0)  /* Emulation */
#define W65816_CF    (1<<0)  /* Carry */
#define W65816_ZF    (1<<1)  /* Zero */
#define W65816_IF    (1<<2)  /* IRQ disable */
#define W65816_DF    (1<<3)  /* Decimal mode */
#define W65816_BF    (1<<4)  /* BRK command (Emulation) */
#define W65816_XF    (1<<4)  /* Index Register Select (Native) */
#define W65816_UF    (1<<5)  /* Unused (Emulated) */
#define W65816_MF    (1<<5)  /* Memory Select (Native) */
#define W65816_VF    (1<<6)  /* Overflow */
#define W65816_NF    (1<<7)  /* Negative */

/* internal BRK state flags */
#define W65816_BRK_IRQ   (1<<0)  /* IRQ was triggered */
#define W65816_BRK_NMI   (1<<1)  /* NMI was triggered */
#define W65816_BRK_RESET (1<<2)  /* RES was triggered */

/* the desc structure provided to w65816_init() */
typedef struct {
    bool bcd_disabled;              /* set to true if BCD mode is disabled */
} w65816_desc_t;

/* CPU state */
typedef struct {
    uint16_t IR;        /* internal Instruction Register */
    uint16_t PC;        /* internal Program Counter register */
    uint16_t AD;        /* ADL/ADH internal register */
    uint16_t C;         /* BA=C accumulator */
    uint16_t X,Y;       /* index registers */
    uint8_t DBR,PBR;    /* Bank registers (Data, Program) */
    uint16_t D;         /* Direct register */
    uint8_t P;          /* Processor status register */
    uint16_t S;         /* Stack pointer */
    uint64_t PINS;      /* last stored pin state (do NOT modify) */
    uint16_t irq_pip;
    uint16_t nmi_pip;
    uint8_t emulation;  /* W65C02 Emulation mode */
    uint8_t brk_flags;  /* W65816_BRK_* */
    uint8_t bcd_enabled;
} w65816_t;

/* initialize a new w65816 instance and return initial pin mask */
uint64_t w65816_init(w65816_t* cpu, const w65816_desc_t* desc);
/* execute one tick */
uint64_t w65816_tick(w65816_t* cpu, uint64_t pins);
// prepare w65816_t snapshot for saving
void w65816_snapshot_onsave(w65816_t* snapshot);
// fixup w65816_t snapshot after loading
void w65816_snapshot_onload(w65816_t* snapshot, w65816_t* sys);

/* register access functions */
void w65816_set_a(w65816_t* cpu, uint8_t v);
void w65816_set_b(w65816_t* cpu, uint8_t v);
void w65816_set_c(w65816_t* cpu, uint16_t v);
void w65816_set_x(w65816_t* cpu, uint16_t v);
void w65816_set_y(w65816_t* cpu, uint16_t v);
void w65816_set_s(w65816_t* cpu, uint16_t v);
void w65816_set_d(w65816_t* cpu, uint16_t v);
void w65816_set_p(w65816_t* cpu, uint8_t v);
void w65816_set_e(w65816_t* cpu, bool v);
void w65816_set_pc(w65816_t* cpu, uint16_t v);
void w65816_set_pb(w65816_t* cpu, uint8_t v);
void w65816_set_db(w65816_t* cpu, uint8_t v);
uint8_t w65816_a(w65816_t* cpu);
uint8_t w65816_b(w65816_t* cpu);
uint16_t w65816_c(w65816_t* cpu);
uint16_t w65816_x(w65816_t* cpu);
uint16_t w65816_y(w65816_t* cpu);
uint16_t w65816_s(w65816_t* cpu);
uint16_t w65816_d(w65816_t* cpu);
uint8_t w65816_p(w65816_t* cpu);
bool w65816_e(w65816_t* cpu);
uint16_t w65816_pc(w65816_t* cpu);
uint8_t w65816_pb(w65816_t* cpu);
uint8_t w65816_db(w65816_t* cpu);

/* extract 24-bit address bus from 64-bit pins */
#define W65816_GET_ADDR(p) ((uint32_t)((p)&0xFFFFULL)|(uint32_t)((p>>16)&0xFF0000ULL))
/* merge 24-bit address bus value into 64-bit pins */
#define W65816_SET_ADDR(p,a) {p=(((p)&~0xFF0000FFFFULL)|((a)&0xFFFFULL)|((a<<16)&0xFF00000000ULL));}
/* extract 8-bit bank value from 64-bit pins */
#define W65816_GET_BANK(p) ((uint8_t)(((p)&0xFF00000000ULL)>>32))
/* merge 8-bit bank value into 64-bit pins */
#define W65816_SET_BANK(p,a) {p=(((p)&~0xFF00000000ULL)|((a<<32)&0xFF00000000ULL));}
/* extract 8-bit data bus from 64-bit pins */
#define W65816_GET_DATA(p) ((uint8_t)(((p)&0xFF0000ULL)>>16))
/* merge 8-bit data bus value into 64-bit pins */
#define W65816_SET_DATA(p,d) {p=(((p)&~0xFF0000ULL)|(((d)<<16)&0xFF0000ULL));}
/* copy data bus value from other pin mask */
#define W65816_COPY_DATA(p0,p1) (((p0)&~0xFF0000ULL)|((p1)&0xFF0000ULL))
/* return a pin mask with control-pins, address and data bus */
#define W65816_MAKE_PINS(ctrl, addr, data) ((ctrl)|(((data)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)|((addr<<16)&0xFF00000000ULL))

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

#if defined(__GNUC__)
#define _W65816_UNREACHABLE __builtin_unreachable()
#elif defined(_MSC_VER)
#define _W65816_UNREACHABLE __assume(0)
#else
#define _W65816_UNREACHABLE
#endif

/* register access macros */
#define _E(c) ((bool)c->emulation)
#define _a8(c) (_E(c)||(bool)(c->P&W65816_MF))
#define _i8(c) (_E(c)||(bool)(c->P&W65816_XF))
#define _A(c) (*(((uint8_t*)(void*)&c->C)))
#define _B(c) (*(((uint8_t*)((void*)&c->C))+1))
#define _C(c) (*((uint16_t*)(&c->C)))
#define _XL(c) (*(((uint8_t*)(void*)&c->X)))
#define _XH(c) (*(((uint8_t*)((void*)&c->X))+1))
#define _X(c) (*(((uint16_t*)(void*)&c->X)))
#define _YL(c) (*(((uint8_t*)(void*)&c->Y)))
#define _YH(c) (*(((uint8_t*)((void*)&c->Y))+1))
#define _Y(c) (*(((uint16_t*)(void*)&c->Y)))
#define _S(c) (*(((uint16_t*)(void*)&c->S)))

/* register access functions */
void w65816_set_a(w65816_t* cpu, uint8_t v) { _A(cpu) = v; }
void w65816_set_b(w65816_t* cpu, uint8_t v) { _B(cpu) = v; }
void w65816_set_c(w65816_t* cpu, uint16_t v) { cpu->C = v; }
void w65816_set_x(w65816_t* cpu, uint16_t v) { cpu->X = v; }
void w65816_set_y(w65816_t* cpu, uint16_t v) { cpu->Y = v; }
void w65816_set_s(w65816_t* cpu, uint16_t v) { cpu->S = v; }
void w65816_set_d(w65816_t* cpu, uint16_t v) { cpu->D = v; }
void w65816_set_p(w65816_t* cpu, uint8_t v) { cpu->P = v; }
void w65816_set_e(w65816_t* cpu, bool v) { cpu->emulation = v; }
void w65816_set_pc(w65816_t* cpu, uint16_t v) { cpu->PC = v; }
void w65816_set_pb(w65816_t* cpu, uint8_t v) { cpu->PBR = v; }
void w65816_set_db(w65816_t* cpu, uint8_t v) { cpu->DBR = v; }
uint8_t w65816_a(w65816_t* cpu) { return _A(cpu); }
uint8_t w65816_b(w65816_t* cpu) { return _B(cpu); }
uint16_t w65816_c(w65816_t* cpu) { return cpu->C; }
uint16_t w65816_x(w65816_t* cpu) { return cpu->X; }
uint16_t w65816_y(w65816_t* cpu) { return cpu->Y; }
uint16_t w65816_s(w65816_t* cpu) { return cpu->S; }
uint16_t w65816_d(w65816_t* cpu) { return cpu->D; }
uint8_t w65816_p(w65816_t* cpu) { return cpu->P; }
bool w65816_e(w65816_t* cpu) { return cpu->emulation; }
uint16_t w65816_pc(w65816_t* cpu) { return cpu->PC; }
uint8_t w65816_pb(w65816_t* cpu) { return cpu->PBR; }
uint8_t w65816_db(w65816_t* cpu) { return cpu->DBR; }

/* helper macros and functions for code-generated instruction decoder */
#define _W65816_NZ(p,v) ((p&~(W65816_NF|W65816_ZF))|((v&0xFF)?(v&W65816_NF):W65816_ZF))
#define _W65816_NZ16(p,v) ((p&~(W65816_NF|W65816_ZF))|((v&0xFFFF)?((v>>8)&W65816_NF):W65816_ZF))

static inline void _w65816_adc(w65816_t* cpu, uint8_t val) {
    if (cpu->bcd_enabled && (cpu->P & W65816_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & W65816_CF ? 1 : 0;
        cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF|W65816_CF);
        uint8_t al = (_A(cpu) & 0x0F) + (val & 0x0F) + c;
        if (al > 9) {
            al += 6;
        }
        uint8_t ah = (_A(cpu) >> 4) + (val >> 4) + (al > 0x0F);
        if (0 == (uint8_t)(_A(cpu) + val + c)) {
            cpu->P |= W65816_ZF;
        }
        else if (ah & 0x08) {
            cpu->P |= W65816_NF;
        }
        if (~(_A(cpu)^val) & (_A(cpu)^(ah<<4)) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (ah > 9) {
            ah += 6;
        }
        if (ah > 15) {
            cpu->P |= W65816_CF;
        }
        _A(cpu) = (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        uint16_t sum = _A(cpu) + val + (cpu->P & W65816_CF ? 1:0);
        cpu->P &= ~(W65816_VF|W65816_CF);
        cpu->P = _W65816_NZ(cpu->P,sum);
        if (~(_A(cpu)^val) & (_A(cpu)^sum) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (sum & 0xFF00) {
            cpu->P |= W65816_CF;
        }
        _A(cpu) = sum & 0xFF;
    }
}

static inline void _w65816_adc16(w65816_t* cpu, uint16_t val) {
    if (cpu->bcd_enabled && (cpu->P & W65816_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & W65816_CF ? 1 : 0;
        cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF|W65816_CF);
        uint8_t al = (_A(cpu) & 0x0F) + (val & 0x0F) + c;
        if (al > 9) {
            al += 6;
        }
        uint8_t ah = (_A(cpu) >> 4) + (val >> 4) + (al > 0x0F);
        if (ah > 9) {
            ah += 6;
        }
        uint8_t bl = (_B(cpu) & 0x0F) + (val >> 8) + (ah > 0x0F);
        if (bl > 9) {
            bl += 6;
        }
        uint8_t bh = (_B(cpu) >> 4) + (val >> 12) + (bl > 0x0F);
        if (0 == (uint8_t)(_C(cpu) + val + c)) {
            cpu->P |= W65816_ZF;
        }
        else if (bh & 0x08) {
            cpu->P |= W65816_NF;
        }
        if (~(_C(cpu)^val) & (_B(cpu)^(bh<<4)) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (bh > 9) {
            bh += 6;
        }
        if (bh > 15) {
            cpu->P |= W65816_CF;
        }
        _C(cpu) = (bh<<12) | (bl<<8) | (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        uint16_t sum = _C(cpu) + val + (cpu->P & W65816_CF ? 1:0);
        cpu->P &= ~(W65816_VF|W65816_CF);
        cpu->P = _W65816_NZ16(cpu->P,sum);
        if (~(_C(cpu)^val) & (_C(cpu)^sum) & 0x8000) {
            cpu->P |= W65816_VF;
        }
        if (sum & 0xFF0000) {
            cpu->P |= W65816_CF;
        }
        _C(cpu) = sum & 0xFFFF;
    }
}

static inline void _w65816_sbc(w65816_t* cpu, uint8_t val) {
    if (cpu->bcd_enabled && (cpu->P & W65816_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & W65816_CF ? 0 : 1;
        cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF|W65816_CF);
        uint16_t diff = _A(cpu) - val - c;
        uint8_t al = (_A(cpu) & 0x0F) - (val & 0x0F) - c;
        if ((int8_t)al < 0) {
            al -= 6;
        }
        uint8_t ah = (_A(cpu)>>4) - (val>>4) - ((int8_t)al < 0);
        if (0 == (uint8_t)diff) {
            cpu->P |= W65816_ZF;
        }
        else if (diff & 0x80) {
            cpu->P |= W65816_NF;
        }
        if ((_A(cpu)^val) & (_A(cpu)^diff) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (!(diff & 0xFF00)) {
            cpu->P |= W65816_CF;
        }
        if (ah & 0x80) {
            ah -= 6;
        }
        _A(cpu) = (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        uint16_t diff = _A(cpu) - val - (cpu->P & W65816_CF ? 0 : 1);
        cpu->P &= ~(W65816_VF|W65816_CF);
        cpu->P = _W65816_NZ(cpu->P, (uint8_t)diff);
        if ((_A(cpu)^val) & (_A(cpu)^diff) & 0x80) {
            cpu->P |= W65816_VF;
        }
        if (!(diff & 0xFF00)) {
            cpu->P |= W65816_CF;
        }
        _A(cpu) = diff & 0xFF;
    }
}

static inline void _w65816_sbc16(w65816_t* cpu, uint16_t val) {
    if (cpu->bcd_enabled && (cpu->P & W65816_DF)) {
        /* decimal mode (credit goes to MAME) */
        uint8_t c = cpu->P & W65816_CF ? 0 : 1;
        cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF|W65816_CF);
        uint16_t diff = _C(cpu) - val - c;
        uint8_t al = (_A(cpu) & 0x0F) - (val & 0x0F) - c;
        if ((int8_t)al < 0) {
            al -= 6;
        }
        uint8_t ah = (_A(cpu)>>4) - (val>>4) - ((int8_t)al < 0);
        if ((int8_t)ah < 0) {
            ah -= 6;
        }
        uint8_t bl = (_A(cpu) & 0x0F) - (val & 0x0F) - ((int8_t)ah < 0);
        if ((int8_t)bl < 0) {
            bl -= 6;
        }
        uint8_t bh = (_A(cpu)>>4) - (val>>4) - ((int8_t)al < 0);
        if (0 == (uint8_t)diff) {
            cpu->P |= W65816_ZF;
        }
        else if (diff & 0x8000) {
            cpu->P |= W65816_NF;
        }
        if ((_C(cpu)^val) & (_C(cpu)^diff) & 0x8000) {
            cpu->P |= W65816_VF;
        }
        if (!(diff & 0xFF0000)) {
            cpu->P |= W65816_CF;
        }
        if (bh & 0x80) {
            bh -= 6;
        }
        _C(cpu) = (bh<<12) | (bl<<8) | (ah<<4) | (al & 0x0F);
    }
    else {
        /* default mode */
        uint16_t diff = _C(cpu) - val - (cpu->P & W65816_CF ? 0 : 1);
        cpu->P &= ~(W65816_VF|W65816_CF);
        cpu->P = _W65816_NZ16(cpu->P, (uint8_t)diff);
        if ((_C(cpu)^val) & (_C(cpu)^diff) & 0x8000) {
            cpu->P |= W65816_VF;
        }
        if (!(diff & 0xFF0000)) {
            cpu->P |= W65816_CF;
        }
        _C(cpu) = diff & 0xFFFF;
    }
}

static inline void _w65816_cmp(w65816_t* cpu, uint8_t r, uint8_t v) {
    uint16_t t = r - v;
    cpu->P = (_W65816_NZ(cpu->P, (uint8_t)t) & ~W65816_CF) | ((t & 0xFF00) ? 0:W65816_CF);
}

static inline void _w65816_cmp16(w65816_t* cpu, uint16_t r, uint16_t v) {
    uint32_t t = r - v;
    cpu->P = (_W65816_NZ16(cpu->P, (uint16_t)t) & ~W65816_CF) | ((t & 0xFF0000) ? 0:W65816_CF);
}

static inline uint8_t _w65816_asl(w65816_t* cpu, uint8_t v) {
    cpu->P = (_W65816_NZ(cpu->P, v<<1) & ~W65816_CF) | ((v & 0x80) ? W65816_CF:0);
    return v<<1;
}

static inline uint16_t _w65816_asl16(w65816_t* cpu, uint16_t v) {
    cpu->P = (_W65816_NZ16(cpu->P, v<<1) & ~W65816_CF) | ((v & 0x8000) ? W65816_CF:0);
    return v<<1;
}

static inline uint8_t _w65816_lsr(w65816_t* cpu, uint8_t v) {
    cpu->P = (_W65816_NZ(cpu->P, v>>1) & ~W65816_CF) | ((v & 0x01) ? W65816_CF:0);
    return v>>1;
}

static inline uint16_t _w65816_lsr16(w65816_t* cpu, uint16_t v) {
    cpu->P = (_W65816_NZ16(cpu->P, v>>1) & ~W65816_CF) | ((v & 0x0001) ? W65816_CF:0);
    return v>>1;
}

static inline uint8_t _w65816_rol(w65816_t* cpu, uint8_t v) {
    bool carry = cpu->P & W65816_CF;
    cpu->P &= ~(W65816_NF|W65816_ZF|W65816_CF);
    if (v & 0x80) {
        cpu->P |= W65816_CF;
    }
    v <<= 1;
    if (carry) {
        v |= 1;
    }
    cpu->P = _W65816_NZ(cpu->P, v);
    return v;
}

static inline uint16_t _w65816_rol16(w65816_t* cpu, uint16_t v) {
    bool carry = cpu->P & W65816_CF;
    cpu->P &= ~(W65816_NF|W65816_ZF|W65816_CF);
    if (v & 0x8000) {
        cpu->P |= W65816_CF;
    }
    v <<= 1;
    if (carry) {
        v |= 1;
    }
    cpu->P = _W65816_NZ16(cpu->P, v);
    return v;
}

static inline uint8_t _w65816_ror(w65816_t* cpu, uint8_t v) {
    bool carry = cpu->P & W65816_CF;
    cpu->P &= ~(W65816_NF|W65816_ZF|W65816_CF);
    if (v & 1) {
        cpu->P |= W65816_CF;
    }
    v >>= 1;
    if (carry) {
        v |= 0x80;
    }
    cpu->P = _W65816_NZ(cpu->P, v);
    return v;
}

static inline uint16_t _w65816_ror16(w65816_t* cpu, uint16_t v) {
    bool carry = cpu->P & W65816_CF;
    cpu->P &= ~(W65816_NF|W65816_ZF|W65816_CF);
    if (v & 1) {
        cpu->P |= W65816_CF;
    }
    v >>= 1;
    if (carry) {
        v |= 0x8000;
    }
    cpu->P = _W65816_NZ16(cpu->P, v);
    return v;
}

static inline void _w65816_bit(w65816_t* cpu, uint8_t v) {
    uint8_t t = _A(cpu) & v;
    cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF);
    if (!t) {
        cpu->P |= W65816_ZF;
    }
    cpu->P |= v & (W65816_NF|W65816_VF);
}

static inline void _w65816_bit16(w65816_t* cpu, uint16_t v) {
    uint16_t t = _C(cpu) & v;
    cpu->P &= ~(W65816_NF|W65816_VF|W65816_ZF);
    if (!t) {
        cpu->P |= W65816_ZF;
    }
    cpu->P |= v & (W65816_NF|W65816_VF);
}

static inline void _w65816_xce(w65816_t* cpu) {
    uint8_t e = cpu->emulation;
    cpu->emulation = cpu->P & W65816_CF;
    cpu->P &= ~W65816_CF;
    if (e) {
        cpu->P |= W65816_CF;
    }

    if (!cpu->emulation) {
        cpu->P |= W65816_MF|W65816_XF;
    }
}

static inline void _w65816_xba(w65816_t* cpu) {
    uint8_t t = _A(cpu);
    _A(cpu) = _B(cpu);
    _B(cpu) = t;
}
#undef _W65816_NZ

uint64_t w65816_init(w65816_t* c, const w65816_desc_t* desc) {
    CHIPS_ASSERT(c && desc);
    memset(c, 0, sizeof(*c));
    c->emulation = true; /* start in Emulation mode */
    c->P = W65816_ZF;
    c->bcd_enabled = !desc->bcd_disabled;
    c->PINS = W65816_RW | W65816_VPA | W65816_VDA | W65816_RES;
    return c->PINS;
}

void w65816_snapshot_onsave(w65816_t* snapshot) {
    CHIPS_ASSERT(snapshot);
}

void w65816_snapshot_onload(w65816_t* snapshot, w65816_t* sys) {
    CHIPS_ASSERT(snapshot && sys);
}

/* set 16-bit address in 64-bit pin mask */
#define _SA(addr) pins=(pins&~0xFFFF)|((addr)&0xFFFFULL)
/* extract 16-bit address from pin mask */
#define _GA() ((uint16_t)(pins&0xFFFFULL))
/* set 16-bit address and 8-bit data in 64-bit pin mask */
#define _SAD(addr,data) pins=(pins&~0xFFFFFF)|((((data)&0xFF)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)
/* set 24-bit address in 64-bit pin mask */
#define _SAL(addr) pins=(pins&~0xFF0000FFFF)|((addr)&0xFFFFULL)|(((addr)<<16)&0xFF00000000ULL)
/* set 8-bit bank address in 64-bit pin mask */
#define _SB(addr) pins=(pins&~0xFF00000000)|(((addr)&0xFFULL)<<32)
/* extract 8-bit bank address from pin mask */
#define _GB() ((uint8_t)((pins>>32)&0xFFULL))
/* extract 24-bit address from pin mask */
#define _GAL() ((uint32_t)((pins&0xFFFFULL)|((pins>>16)&0xFF0000ULL)))
/* set 24-bit address and 8-bit data in 64-bit pin mask */
#define _SALD(addr,data) pins=(pins&~0xFFFFFF)|((((data)&0xFF)<<16)&0xFF0000ULL)|((addr)&0xFFFFULL)|(((addr)>>16)&0xFF0000ULL)
/* fetch next opcode byte */
#define _FETCH() _VPA();_VDA(c->PBR);_SA(c->PC);
/* signal valid program address */
#define _VPA() _ON(W65816_VPA);_SB(c->PBR);
/* signal valid data address */
#define _VDA(bank) _ON(W65816_VDA);_SB(bank);
/* set 8-bit data in 64-bit pin mask */
#define _SD(data) pins=((pins&~0xFF0000ULL)|(((data&0xFF)<<16)&0xFF0000ULL))
/* extract 8-bit data from 64-bit pin mask */
#define _GD() ((uint8_t)((pins&0xFF0000ULL)>>16))
/* enable control pins */
#define _ON(m) pins|=(m)
/* disable control pins */
#define _OFF(m) pins&=~(m)
/* a memory read tick */
#define _RD() _ON(W65816_RW);
/* a memory write tick */
#define _WR() _OFF(W65816_RW);
/* set N and Z flags depending on value */
#define _NZ(v) c->P=((c->P&~(W65816_NF|W65816_ZF))|((v&0xFF)?(v&W65816_NF):W65816_ZF))
#define _NZ16(v) c->P=((c->P&~(W65816_NF|W65816_ZF))|((v&0xFFFF)?((v>>8)&W65816_NF):W65816_ZF))
/* set Z flag depending on value */
#define _Z(v) c->P=((c->P&~W65816_ZF)|((v&0xFF)?(0):W65816_ZF))
#define _Z16(v) c->P=((c->P&~W65816_ZF)|((v&0xFFFF)?(0):W65816_ZF))
/* get full 16-bit stack pointer */
#define _SP(v) (_E(c)?(0x0100|(v&0xFF)):(v))

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4244)   /* conversion from 'uint16_t' to 'uint8_t', possible loss of data */
#endif

uint64_t w65816_tick(w65816_t* c, uint64_t pins) {
    if (pins & (W65816_VPA|W65816_VDA|W65816_IRQ|W65816_NMI|W65816_RDY|W65816_RES)) {
        // interrupt detection also works in RDY phases, but only NMI is "sticky"

        // NMI is edge-triggered
        if (0 != ((pins & (pins ^ c->PINS)) & W65816_NMI)) {
            c->nmi_pip |= 0x100;
        }
        // IRQ test is level triggered
        if ((pins & W65816_IRQ) && (0 == (c->P & W65816_IF))) {
            c->irq_pip |= 0x100;
        }

        // RDY pin is only checked during read cycles
        if ((pins & (W65816_RW|W65816_RDY)) == (W65816_RW|W65816_RDY)) {
            c->PINS = pins;
            c->irq_pip <<= 1;
            return pins;
        }
        if ((pins & W65816_VPA) && (pins & W65816_VDA)) {
            // load new instruction into 'instruction register' and restart tick counter
            c->IR = _GD()<<4;

            // check IRQ, NMI and RES state
            //  - IRQ is level-triggered and must be active in the full cycle
            //    before SYNC
            //  - NMI is edge-triggered, and the change must have happened in
            //    any cycle before SYNC
            //  - RES behaves slightly different than on a real 65816, we go
            //    into RES state as soon as the pin goes active, from there
            //    on, behaviour is 'standard'
            if (0 != (c->irq_pip & 0x400)) {
                c->brk_flags |= W65816_BRK_IRQ;
            }
            if (0 != (c->nmi_pip & 0xFC00)) {
                c->brk_flags |= W65816_BRK_NMI;
            }
            if (0 != (pins & W65816_RES)) {
                c->brk_flags |= W65816_BRK_RESET;
            }
            c->irq_pip &= 0x3FF;
            c->nmi_pip &= 0x3FF;

            // if interrupt or reset was requested, force a BRK instruction
            if (c->brk_flags) {
                c->IR = 0;
                if (c->emulation) c->P &= ~W65816_BF;
                pins &= ~W65816_RES;
            }
            else {
                c->PC++;
            }
        }
        // internal operation is default
        _OFF(W65816_VPA|W65816_VDA);
    }
    // reads are default, writes are special
    _RD();
    switch (c->IR++) {
    // <% decoder
    /* BRK s */
        case (0x00<<4)|0: if(0==c->brk_flags){_VPA();}_SA(c->PC);break;
        case (0x00<<4)|1: _VDA(0);if(0==(c->brk_flags&(W65816_BRK_IRQ|W65816_BRK_NMI))){c->PC++;}if(_E(c)){_SAD(_SP(_S(c)--),c->PC>>8);c->IR++;}else{_SAD(_SP(_S(c)--),c->PBR);c->PBR=0;}if(0==(c->brk_flags&W65816_BRK_RESET)){_WR();}else{c->emulation=true;}break;
        case (0x00<<4)|2: _VDA(0);_SAD(_SP(_S(c)--),c->PC>>8);if(0==(c->brk_flags&W65816_BRK_RESET)){_WR();}break;
        case (0x00<<4)|3: _VDA(0);_SAD(_SP(_S(c)--),c->PC);if(0==(c->brk_flags&W65816_BRK_RESET)){_WR();}break;
        case (0x00<<4)|4: _VDA(0);_SAD(_SP(_S(c)--),(_E(c)?c->P|W65816_UF:c->P));if(c->brk_flags&W65816_BRK_RESET){c->AD=0xFFFC;}else{_WR();if(c->brk_flags&W65816_BRK_NMI){c->AD=_E(c)?0xFFFA:0xFFEA;}else{c->AD=_E(c)?0xFFFE:(c->brk_flags&(W65816_BRK_IRQ)?0xFFEE:0xFFE6);}}break;
        case (0x00<<4)|5: _VDA(0);_SA(c->AD++);c->P|=(W65816_IF);if(_E(c)&&(c->brk_flags&W65816_BRK_IRQ)){c->P|=(W65816_BF);}c->P&=~W65816_DF;c->brk_flags=0; /* RES/NMI hijacking */break;
        case (0x00<<4)|6: _VDA(0);_SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */break;
        case (0x00<<4)|7: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x00<<4)|8: assert(false);break;
    /* ORA (d,x) */
        case (0x01<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x01<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0x01<<4)|2: _SA(c->PC);break;
        case (0x01<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0x01<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0x01<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x01<<4)|6: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x01<<4)|7: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x01<<4)|8: assert(false);break;
    /* COP s */
        case (0x02<<4)|0: if(0==c->brk_flags){_VPA();}_SA(c->PC);break;
        case (0x02<<4)|1: _VDA(0);if(_E(c)){_SAD(_SP(_S(c)--),c->PC>>8);c->IR++;}else{_SAD(_SP(_S(c)--),c->PBR);c->PBR=0;}_WR();break;
        case (0x02<<4)|2: _VDA(0);_SAD(_SP(_S(c)--),c->PC>>8);_WR();break;
        case (0x02<<4)|3: _VDA(0);_SAD(_SP(_S(c)--),c->PC);_WR();break;
        case (0x02<<4)|4: _VDA(0);_SAD(_SP(_S(c)--),(_E(c)?c->P|W65816_UF:c->P));_WR();c->AD=_E(c)?0xFFF4:0xFFE4;break;
        case (0x02<<4)|5: _VDA(0);_SA(c->AD++);c->P|=W65816_IF;c->P&=~W65816_DF;c->brk_flags=0; /* RES/NMI hijacking */break;
        case (0x02<<4)|6: _VDA(0);_SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */break;
        case (0x02<<4)|7: c->PC=(_GD()<<8)|c->AD;break;
        case (0x02<<4)|8: _FETCH();break;
    /* ORA d,s */
        case (0x03<<4)|0: /* (unimpl) */;break;
        case (0x03<<4)|1: break;
        case (0x03<<4)|2: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x03<<4)|3: _B(c)|=_GD();_NZ16(_C(c));break;
        case (0x03<<4)|4: _FETCH();break;
        case (0x03<<4)|5: assert(false);break;
        case (0x03<<4)|6: assert(false);break;
        case (0x03<<4)|7: assert(false);break;
        case (0x03<<4)|8: assert(false);break;
    /* TSB d */
        case (0x04<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x04<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x04<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x04<<4)|3: c->AD=_GD();                    if(_a8(c)){ if(_E(c)){_WR();} }else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x04<<4)|4: if(_a8(c)){_VDA(_GB());_SD(_A(c)|c->AD);_WR();_Z(_A(c)&c->AD);}else{c->AD|=_GD()<<8;}break;
        case (0x04<<4)|5: if(_a8(c)){_FETCH();                                          }else{_VDA(_GB());_SD(_B(c)|(c->AD>>8));_WR();_Z16(_C(c)&c->AD);}break;
        case (0x04<<4)|6: _VDA(_GB());_SALD(_GAL()-1,_A(c)|(c->AD&0xFF));_WR();break;
        case (0x04<<4)|7: _FETCH();break;
        case (0x04<<4)|8: assert(false);break;
    /* ORA d */
        case (0x05<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x05<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x05<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x05<<4)|3: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x05<<4)|4: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x05<<4)|5: assert(false);break;
        case (0x05<<4)|6: assert(false);break;
        case (0x05<<4)|7: assert(false);break;
        case (0x05<<4)|8: assert(false);break;
    /* ASL d */
        case (0x06<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x06<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x06<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x06<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x06<<4)|4: c->AD|=_GD()<<8;break;
        case (0x06<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_asl(c,c->AD));}else{c->AD=_w65816_asl16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x06<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x06<<4)|7: _FETCH();break;
        case (0x06<<4)|8: assert(false);break;
    /* ORA [d] */
        case (0x07<<4)|0: /* (unimpl) */;break;
        case (0x07<<4)|1: break;
        case (0x07<<4)|2: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x07<<4)|3: _B(c)|=_GD();_NZ16(_C(c));break;
        case (0x07<<4)|4: _FETCH();break;
        case (0x07<<4)|5: assert(false);break;
        case (0x07<<4)|6: assert(false);break;
        case (0x07<<4)|7: assert(false);break;
        case (0x07<<4)|8: assert(false);break;
    /* PHP s */
        case (0x08<<4)|0: _SA(c->PC);break;
        case (0x08<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),(_E(c)?c->P|W65816_UF:c->P));_WR();break;
        case (0x08<<4)|2: _FETCH();break;
        case (0x08<<4)|3: assert(false);break;
        case (0x08<<4)|4: assert(false);break;
        case (0x08<<4)|5: assert(false);break;
        case (0x08<<4)|6: assert(false);break;
        case (0x08<<4)|7: assert(false);break;
        case (0x08<<4)|8: assert(false);break;
    /* ORA # */
        case (0x09<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x09<<4)|1: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0x09<<4)|2: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x09<<4)|3: assert(false);break;
        case (0x09<<4)|4: assert(false);break;
        case (0x09<<4)|5: assert(false);break;
        case (0x09<<4)|6: assert(false);break;
        case (0x09<<4)|7: assert(false);break;
        case (0x09<<4)|8: assert(false);break;
    /* ASL A */
        case (0x0A<<4)|0: _SA(c->PC);break;
        case (0x0A<<4)|1: if(_a8(c)){_A(c)=_w65816_asl(c,_A(c));}else{_C(c)=_w65816_asl16(c,_C(c));}_FETCH();break;
        case (0x0A<<4)|2: assert(false);break;
        case (0x0A<<4)|3: assert(false);break;
        case (0x0A<<4)|4: assert(false);break;
        case (0x0A<<4)|5: assert(false);break;
        case (0x0A<<4)|6: assert(false);break;
        case (0x0A<<4)|7: assert(false);break;
        case (0x0A<<4)|8: assert(false);break;
    /* PHD s */
        case (0x0B<<4)|0: _SA(c->PC);break;
        case (0x0B<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),c->D>>8);_WR();break;
        case (0x0B<<4)|2: _VDA(0);_SAD(_SP(_S(c)--),c->D);_WR();break;
        case (0x0B<<4)|3: _FETCH();break;
        case (0x0B<<4)|4: assert(false);break;
        case (0x0B<<4)|5: assert(false);break;
        case (0x0B<<4)|6: assert(false);break;
        case (0x0B<<4)|7: assert(false);break;
        case (0x0B<<4)|8: assert(false);break;
    /* TSB a */
        case (0x0C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x0C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x0C<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x0C<<4)|3: c->AD=_GD();                    if(_a8(c)){ if(_E(c)){_WR();} }else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x0C<<4)|4: if(_a8(c)){_VDA(_GB());_SD(_A(c)|c->AD);_WR();_Z(_A(c)&c->AD);}else{c->AD|=_GD()<<8;}break;
        case (0x0C<<4)|5: if(_a8(c)){_FETCH();                                          }else{_VDA(_GB());_SD(_B(c)|(c->AD>>8));_WR();_Z16(_C(c)&c->AD);}break;
        case (0x0C<<4)|6: _VDA(_GB());_SALD(_GAL()-1,_A(c)|(c->AD&0xFF));_WR();break;
        case (0x0C<<4)|7: _FETCH();break;
        case (0x0C<<4)|8: assert(false);break;
    /* ORA a */
        case (0x0D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x0D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x0D<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x0D<<4)|3: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x0D<<4)|4: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x0D<<4)|5: assert(false);break;
        case (0x0D<<4)|6: assert(false);break;
        case (0x0D<<4)|7: assert(false);break;
        case (0x0D<<4)|8: assert(false);break;
    /* ASL a */
        case (0x0E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x0E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x0E<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x0E<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x0E<<4)|4: c->AD|=_GD()<<8;break;
        case (0x0E<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_asl(c,c->AD));}else{c->AD=_w65816_asl16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x0E<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x0E<<4)|7: _FETCH();break;
        case (0x0E<<4)|8: assert(false);break;
    /* ORA al */
        case (0x0F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x0F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x0F<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0x0F<<4)|3: _VDA(_GD());_SA(c->AD);break;
        case (0x0F<<4)|4: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x0F<<4)|5: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x0F<<4)|6: assert(false);break;
        case (0x0F<<4)|7: assert(false);break;
        case (0x0F<<4)|8: assert(false);break;
    /* BPL r */
        case (0x10<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x10<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x80)!=0x0){_FETCH();};break;
        case (0x10<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x10<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0x10<<4)|4: assert(false);break;
        case (0x10<<4)|5: assert(false);break;
        case (0x10<<4)|6: assert(false);break;
        case (0x10<<4)|7: assert(false);break;
        case (0x10<<4)|8: assert(false);break;
    /* ORA (d),y */
        case (0x11<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x11<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0x11<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0x11<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x11<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x11<<4)|5: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x11<<4)|6: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x11<<4)|7: assert(false);break;
        case (0x11<<4)|8: assert(false);break;
    /* ORA (d) */
        case (0x12<<4)|0: /* (unimpl) */;break;
        case (0x12<<4)|1: break;
        case (0x12<<4)|2: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x12<<4)|3: _B(c)|=_GD();_NZ16(_C(c));break;
        case (0x12<<4)|4: _FETCH();break;
        case (0x12<<4)|5: assert(false);break;
        case (0x12<<4)|6: assert(false);break;
        case (0x12<<4)|7: assert(false);break;
        case (0x12<<4)|8: assert(false);break;
    /* ORA (d,s),y */
        case (0x13<<4)|0: /* (unimpl) */;break;
        case (0x13<<4)|1: break;
        case (0x13<<4)|2: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x13<<4)|3: _B(c)|=_GD();_NZ16(_C(c));break;
        case (0x13<<4)|4: _FETCH();break;
        case (0x13<<4)|5: assert(false);break;
        case (0x13<<4)|6: assert(false);break;
        case (0x13<<4)|7: assert(false);break;
        case (0x13<<4)|8: assert(false);break;
    /* TRB d */
        case (0x14<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x14<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x14<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x14<<4)|3: c->AD=_GD();                     if(_a8(c)){ if(_E(c)){_WR();} }else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x14<<4)|4: if(_a8(c)){_VDA(_GB());_SD(~_A(c)&c->AD);_WR();_Z(_A(c)&c->AD);}else{c->AD|=_GD()<<8;}break;
        case (0x14<<4)|5: if(_a8(c)){_FETCH();                                           }else{_VDA(_GB());_SD(~_B(c)&(c->AD>>8));_WR();_Z16(_C(c)&c->AD);}break;
        case (0x14<<4)|6: _VDA(_GB());_SALD(_GAL()-1,~_A(c)&(c->AD&0xFF));_WR();break;
        case (0x14<<4)|7: _FETCH();break;
        case (0x14<<4)|8: assert(false);break;
    /* ORA d,x */
        case (0x15<<4)|0: _VPA();_SA(c->PC);break;
        case (0x15<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x15<<4)|2: _SA(c->PC++);break;
        case (0x15<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x15<<4)|4: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x15<<4)|5: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x15<<4)|6: assert(false);break;
        case (0x15<<4)|7: assert(false);break;
        case (0x15<<4)|8: assert(false);break;
    /* ASL d,x */
        case (0x16<<4)|0: _VPA();_SA(c->PC);break;
        case (0x16<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x16<<4)|2: _SA(c->PC++);break;
        case (0x16<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x16<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x16<<4)|5: c->AD|=_GD()<<8;break;
        case (0x16<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_asl(c,c->AD));}else{c->AD=_w65816_asl16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x16<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x16<<4)|8: _FETCH();break;
    /* ORA [d],y */
        case (0x17<<4)|0: /* (unimpl) */;break;
        case (0x17<<4)|1: break;
        case (0x17<<4)|2: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x17<<4)|3: _B(c)|=_GD();_NZ16(_C(c));break;
        case (0x17<<4)|4: _FETCH();break;
        case (0x17<<4)|5: assert(false);break;
        case (0x17<<4)|6: assert(false);break;
        case (0x17<<4)|7: assert(false);break;
        case (0x17<<4)|8: assert(false);break;
    /* CLE i */
        case (0x18<<4)|0: _SA(c->PC);break;
        case (0x18<<4)|1: c->P&=~0x1;_FETCH();break;
        case (0x18<<4)|2: assert(false);break;
        case (0x18<<4)|3: assert(false);break;
        case (0x18<<4)|4: assert(false);break;
        case (0x18<<4)|5: assert(false);break;
        case (0x18<<4)|6: assert(false);break;
        case (0x18<<4)|7: assert(false);break;
        case (0x18<<4)|8: assert(false);break;
    /* ORA a,y */
        case (0x19<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x19<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x19<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x19<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x19<<4)|4: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x19<<4)|5: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x19<<4)|6: assert(false);break;
        case (0x19<<4)|7: assert(false);break;
        case (0x19<<4)|8: assert(false);break;
    /* INC A */
        case (0x1A<<4)|0: _SA(c->PC);break;
        case (0x1A<<4)|1: if(_a8(c)){_A(c)++;_NZ(_A(c));}else{_C(c)++;_NZ16(_C(c));}_FETCH();break;
        case (0x1A<<4)|2: assert(false);break;
        case (0x1A<<4)|3: assert(false);break;
        case (0x1A<<4)|4: assert(false);break;
        case (0x1A<<4)|5: assert(false);break;
        case (0x1A<<4)|6: assert(false);break;
        case (0x1A<<4)|7: assert(false);break;
        case (0x1A<<4)|8: assert(false);break;
    /* TCS i */
        case (0x1B<<4)|0: _SA(c->PC);break;
        case (0x1B<<4)|1: c->S=c->C;_FETCH();break;
        case (0x1B<<4)|2: assert(false);break;
        case (0x1B<<4)|3: assert(false);break;
        case (0x1B<<4)|4: assert(false);break;
        case (0x1B<<4)|5: assert(false);break;
        case (0x1B<<4)|6: assert(false);break;
        case (0x1B<<4)|7: assert(false);break;
        case (0x1B<<4)|8: assert(false);break;
    /* TRB a */
        case (0x1C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x1C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x1C<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x1C<<4)|3: c->AD=_GD();                     if(_a8(c)){ if(_E(c)){_WR();} }else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x1C<<4)|4: if(_a8(c)){_VDA(_GB());_SD(~_A(c)&c->AD);_WR();_Z(_A(c)&c->AD);}else{c->AD|=_GD()<<8;}break;
        case (0x1C<<4)|5: if(_a8(c)){_FETCH();                                           }else{_VDA(_GB());_SD(~_B(c)&(c->AD>>8));_WR();_Z16(_C(c)&c->AD);}break;
        case (0x1C<<4)|6: _VDA(_GB());_SALD(_GAL()-1,~_A(c)&(c->AD&0xFF));_WR();break;
        case (0x1C<<4)|7: _FETCH();break;
        case (0x1C<<4)|8: assert(false);break;
    /* ORA a,x */
        case (0x1D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x1D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x1D<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x1D<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x1D<<4)|4: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x1D<<4)|5: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x1D<<4)|6: assert(false);break;
        case (0x1D<<4)|7: assert(false);break;
        case (0x1D<<4)|8: assert(false);break;
    /* ASL a,x */
        case (0x1E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x1E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x1E<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0x1E<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x1E<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x1E<<4)|5: c->AD|=_GD()<<8;break;
        case (0x1E<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_asl(c,c->AD));}else{c->AD=_w65816_asl16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x1E<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x1E<<4)|8: _FETCH();break;
    /* ORA al,x */
        case (0x1F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x1F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x1F<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0x1F<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));break;
        case (0x1F<<4)|4: _A(c)|=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x1F<<4)|5: _B(c)|=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x1F<<4)|6: assert(false);break;
        case (0x1F<<4)|7: assert(false);break;
        case (0x1F<<4)|8: assert(false);break;
    /* JSR a */
        case (0x20<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x20<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x20<<4)|2: _SA(c->PC);c->AD=(_GD()<<8)|c->AD;break;
        case (0x20<<4)|3: _VDA(0);_SAD(_SP(_S(c)--),c->PC>>8);_WR();break;
        case (0x20<<4)|4: _VDA(0);_SAD(_SP(_S(c)--),c->PC);_WR();break;
        case (0x20<<4)|5: c->PC=c->AD;_FETCH();break;
        case (0x20<<4)|6: assert(false);break;
        case (0x20<<4)|7: assert(false);break;
        case (0x20<<4)|8: assert(false);break;
    /* AND (d,x) */
        case (0x21<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x21<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0x21<<4)|2: _SA(c->PC);break;
        case (0x21<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0x21<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0x21<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x21<<4)|6: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x21<<4)|7: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x21<<4)|8: assert(false);break;
    /* JSL a */
        case (0x22<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x22<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x22<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x22<<4)|3: _VDA(0);_SAD(_SP(_S(c)),c->PBR);_WR();break;
        case (0x22<<4)|4: _SA(_SP(_S(c)--));c->AD=(_GD()<<8)|c->AD;break;
        case (0x22<<4)|5: _VPA();_SA(c->PC++);break;
        case (0x22<<4)|6: _VDA(0);_SAD(_SP(_S(c)--),c->PC>>8);c->PBR=_GD();_WR();break;
        case (0x22<<4)|7: _VDA(0);_SAD(_SP(_S(c)--),c->PC);_WR();break;
        case (0x22<<4)|8: c->PC=c->AD;_FETCH();break;
    /* AND d,s */
        case (0x23<<4)|0: /* (unimpl) */;break;
        case (0x23<<4)|1: break;
        case (0x23<<4)|2: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x23<<4)|3: _B(c)&=_GD();_NZ16(_C(c));break;
        case (0x23<<4)|4: _FETCH();break;
        case (0x23<<4)|5: assert(false);break;
        case (0x23<<4)|6: assert(false);break;
        case (0x23<<4)|7: assert(false);break;
        case (0x23<<4)|8: assert(false);break;
    /* BIT d */
        case (0x24<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x24<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x24<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x24<<4)|3: if(_a8(c)){_w65816_bit(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x24<<4)|4: _w65816_bit16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x24<<4)|5: assert(false);break;
        case (0x24<<4)|6: assert(false);break;
        case (0x24<<4)|7: assert(false);break;
        case (0x24<<4)|8: assert(false);break;
    /* AND d */
        case (0x25<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x25<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x25<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x25<<4)|3: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x25<<4)|4: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x25<<4)|5: assert(false);break;
        case (0x25<<4)|6: assert(false);break;
        case (0x25<<4)|7: assert(false);break;
        case (0x25<<4)|8: assert(false);break;
    /* ROL d */
        case (0x26<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x26<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x26<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x26<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x26<<4)|4: c->AD|=_GD()<<8;break;
        case (0x26<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_rol(c,c->AD));}else{c->AD=_w65816_rol16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x26<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x26<<4)|7: _FETCH();break;
        case (0x26<<4)|8: assert(false);break;
    /* AND [d] */
        case (0x27<<4)|0: /* (unimpl) */;break;
        case (0x27<<4)|1: break;
        case (0x27<<4)|2: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x27<<4)|3: _B(c)&=_GD();_NZ16(_C(c));break;
        case (0x27<<4)|4: _FETCH();break;
        case (0x27<<4)|5: assert(false);break;
        case (0x27<<4)|6: assert(false);break;
        case (0x27<<4)|7: assert(false);break;
        case (0x27<<4)|8: assert(false);break;
    /* PLP s */
        case (0x28<<4)|0: _SA(c->PC);break;
        case (0x28<<4)|1: _SA(c->PC);break;
        case (0x28<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0x28<<4)|3: c->P=_GD();if(_E(c))c->P=(c->P|W65816_BF)&~W65816_UF;_FETCH();break;
        case (0x28<<4)|4: assert(false);break;
        case (0x28<<4)|5: assert(false);break;
        case (0x28<<4)|6: assert(false);break;
        case (0x28<<4)|7: assert(false);break;
        case (0x28<<4)|8: assert(false);break;
    /* AND # */
        case (0x29<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x29<<4)|1: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0x29<<4)|2: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x29<<4)|3: assert(false);break;
        case (0x29<<4)|4: assert(false);break;
        case (0x29<<4)|5: assert(false);break;
        case (0x29<<4)|6: assert(false);break;
        case (0x29<<4)|7: assert(false);break;
        case (0x29<<4)|8: assert(false);break;
    /* ROL A */
        case (0x2A<<4)|0: _SA(c->PC);break;
        case (0x2A<<4)|1: if(_a8(c)){_A(c)=_w65816_rol(c,_A(c));}else{_C(c)=_w65816_rol16(c,_C(c));}_FETCH();break;
        case (0x2A<<4)|2: assert(false);break;
        case (0x2A<<4)|3: assert(false);break;
        case (0x2A<<4)|4: assert(false);break;
        case (0x2A<<4)|5: assert(false);break;
        case (0x2A<<4)|6: assert(false);break;
        case (0x2A<<4)|7: assert(false);break;
        case (0x2A<<4)|8: assert(false);break;
    /* PLD s */
        case (0x2B<<4)|0: _SA(c->PC);break;
        case (0x2B<<4)|1: _SA(c->PC);break;
        case (0x2B<<4)|2: _VDA(0);_SA(_SP(_S(c)++));break;
        case (0x2B<<4)|3: _VDA(0);_SA(_SP(_S(c)));c->AD=_GD();break;
        case (0x2B<<4)|4: c->D=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x2B<<4)|5: assert(false);break;
        case (0x2B<<4)|6: assert(false);break;
        case (0x2B<<4)|7: assert(false);break;
        case (0x2B<<4)|8: assert(false);break;
    /* BIT a */
        case (0x2C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x2C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x2C<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x2C<<4)|3: if(_a8(c)){_w65816_bit(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x2C<<4)|4: _w65816_bit16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x2C<<4)|5: assert(false);break;
        case (0x2C<<4)|6: assert(false);break;
        case (0x2C<<4)|7: assert(false);break;
        case (0x2C<<4)|8: assert(false);break;
    /* AND a */
        case (0x2D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x2D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x2D<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x2D<<4)|3: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x2D<<4)|4: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x2D<<4)|5: assert(false);break;
        case (0x2D<<4)|6: assert(false);break;
        case (0x2D<<4)|7: assert(false);break;
        case (0x2D<<4)|8: assert(false);break;
    /* ROL a */
        case (0x2E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x2E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x2E<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x2E<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x2E<<4)|4: c->AD|=_GD()<<8;break;
        case (0x2E<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_rol(c,c->AD));}else{c->AD=_w65816_rol16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x2E<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x2E<<4)|7: _FETCH();break;
        case (0x2E<<4)|8: assert(false);break;
    /* AND al */
        case (0x2F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x2F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x2F<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0x2F<<4)|3: _VDA(_GD());_SA(c->AD);break;
        case (0x2F<<4)|4: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x2F<<4)|5: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x2F<<4)|6: assert(false);break;
        case (0x2F<<4)|7: assert(false);break;
        case (0x2F<<4)|8: assert(false);break;
    /* BMI r */
        case (0x30<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x30<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x80)!=0x80){_FETCH();};break;
        case (0x30<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x30<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0x30<<4)|4: assert(false);break;
        case (0x30<<4)|5: assert(false);break;
        case (0x30<<4)|6: assert(false);break;
        case (0x30<<4)|7: assert(false);break;
        case (0x30<<4)|8: assert(false);break;
    /* AND (d),y */
        case (0x31<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x31<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0x31<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0x31<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x31<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x31<<4)|5: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x31<<4)|6: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x31<<4)|7: assert(false);break;
        case (0x31<<4)|8: assert(false);break;
    /* AND (d) */
        case (0x32<<4)|0: /* (unimpl) */;break;
        case (0x32<<4)|1: break;
        case (0x32<<4)|2: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x32<<4)|3: _B(c)&=_GD();_NZ16(_C(c));break;
        case (0x32<<4)|4: _FETCH();break;
        case (0x32<<4)|5: assert(false);break;
        case (0x32<<4)|6: assert(false);break;
        case (0x32<<4)|7: assert(false);break;
        case (0x32<<4)|8: assert(false);break;
    /* AND (d,s),y */
        case (0x33<<4)|0: /* (unimpl) */;break;
        case (0x33<<4)|1: break;
        case (0x33<<4)|2: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x33<<4)|3: _B(c)&=_GD();_NZ16(_C(c));break;
        case (0x33<<4)|4: _FETCH();break;
        case (0x33<<4)|5: assert(false);break;
        case (0x33<<4)|6: assert(false);break;
        case (0x33<<4)|7: assert(false);break;
        case (0x33<<4)|8: assert(false);break;
    /* BIT d,x */
        case (0x34<<4)|0: _VPA();_SA(c->PC);break;
        case (0x34<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x34<<4)|2: _SA(c->PC++);break;
        case (0x34<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x34<<4)|4: if(_a8(c)){_w65816_bit(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x34<<4)|5: _w65816_bit16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x34<<4)|6: assert(false);break;
        case (0x34<<4)|7: assert(false);break;
        case (0x34<<4)|8: assert(false);break;
    /* AND d,x */
        case (0x35<<4)|0: _VPA();_SA(c->PC);break;
        case (0x35<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x35<<4)|2: _SA(c->PC++);break;
        case (0x35<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x35<<4)|4: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x35<<4)|5: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x35<<4)|6: assert(false);break;
        case (0x35<<4)|7: assert(false);break;
        case (0x35<<4)|8: assert(false);break;
    /* ROL d,x */
        case (0x36<<4)|0: _VPA();_SA(c->PC);break;
        case (0x36<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x36<<4)|2: _SA(c->PC++);break;
        case (0x36<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x36<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x36<<4)|5: c->AD|=_GD()<<8;break;
        case (0x36<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_rol(c,c->AD));}else{c->AD=_w65816_rol16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x36<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x36<<4)|8: _FETCH();break;
    /* AND [d],y */
        case (0x37<<4)|0: /* (unimpl) */;break;
        case (0x37<<4)|1: break;
        case (0x37<<4)|2: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x37<<4)|3: _B(c)&=_GD();_NZ16(_C(c));break;
        case (0x37<<4)|4: _FETCH();break;
        case (0x37<<4)|5: assert(false);break;
        case (0x37<<4)|6: assert(false);break;
        case (0x37<<4)|7: assert(false);break;
        case (0x37<<4)|8: assert(false);break;
    /* SEE i */
        case (0x38<<4)|0: _SA(c->PC);break;
        case (0x38<<4)|1: c->P|=0x1;_FETCH();break;
        case (0x38<<4)|2: assert(false);break;
        case (0x38<<4)|3: assert(false);break;
        case (0x38<<4)|4: assert(false);break;
        case (0x38<<4)|5: assert(false);break;
        case (0x38<<4)|6: assert(false);break;
        case (0x38<<4)|7: assert(false);break;
        case (0x38<<4)|8: assert(false);break;
    /* AND a,y */
        case (0x39<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x39<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x39<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x39<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x39<<4)|4: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x39<<4)|5: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x39<<4)|6: assert(false);break;
        case (0x39<<4)|7: assert(false);break;
        case (0x39<<4)|8: assert(false);break;
    /* DEC A */
        case (0x3A<<4)|0: _SA(c->PC);break;
        case (0x3A<<4)|1: if(_a8(c)){_A(c)--;_NZ(_A(c));}else{_C(c)--;_NZ16(_C(c));}_FETCH();break;
        case (0x3A<<4)|2: assert(false);break;
        case (0x3A<<4)|3: assert(false);break;
        case (0x3A<<4)|4: assert(false);break;
        case (0x3A<<4)|5: assert(false);break;
        case (0x3A<<4)|6: assert(false);break;
        case (0x3A<<4)|7: assert(false);break;
        case (0x3A<<4)|8: assert(false);break;
    /* TSC i */
        case (0x3B<<4)|0: _SA(c->PC);break;
        case (0x3B<<4)|1: c->C=c->S;_NZ(c->C);break;
        case (0x3B<<4)|2: _FETCH();break;
        case (0x3B<<4)|3: assert(false);break;
        case (0x3B<<4)|4: assert(false);break;
        case (0x3B<<4)|5: assert(false);break;
        case (0x3B<<4)|6: assert(false);break;
        case (0x3B<<4)|7: assert(false);break;
        case (0x3B<<4)|8: assert(false);break;
    /* BIT a,x */
        case (0x3C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x3C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x3C<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x3C<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x3C<<4)|4: if(_a8(c)){_w65816_bit(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x3C<<4)|5: _w65816_bit16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x3C<<4)|6: assert(false);break;
        case (0x3C<<4)|7: assert(false);break;
        case (0x3C<<4)|8: assert(false);break;
    /* AND a,x */
        case (0x3D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x3D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x3D<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x3D<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x3D<<4)|4: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x3D<<4)|5: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x3D<<4)|6: assert(false);break;
        case (0x3D<<4)|7: assert(false);break;
        case (0x3D<<4)|8: assert(false);break;
    /* ROL a,x */
        case (0x3E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x3E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x3E<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0x3E<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x3E<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x3E<<4)|5: c->AD|=_GD()<<8;break;
        case (0x3E<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_rol(c,c->AD));}else{c->AD=_w65816_rol16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x3E<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x3E<<4)|8: _FETCH();break;
    /* AND al,x */
        case (0x3F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x3F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x3F<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0x3F<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));break;
        case (0x3F<<4)|4: _A(c)&=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x3F<<4)|5: _B(c)&=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x3F<<4)|6: assert(false);break;
        case (0x3F<<4)|7: assert(false);break;
        case (0x3F<<4)|8: assert(false);break;
    /* RTI s */
        case (0x40<<4)|0: _SA(c->PC);break;
        case (0x40<<4)|1: _SA(c->PC);break;
        case (0x40<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0x40<<4)|3: _VDA(0);_SA(_SP(++_S(c)));c->P=_GD();if(_E(c))c->P=(c->P|W65816_BF)&~W65816_UF;break;
        case (0x40<<4)|4: _VDA(0);_SA(_SP(++_S(c)));c->AD=_GD();break;
        case (0x40<<4)|5: c->PC=(_GD()<<8)|c->AD;if(_E(c)){_FETCH();}else{_VDA(0);_SA(_SP(++_S(c)));}break;
        case (0x40<<4)|6: _VDA(0);c->PBR=_GD();break;
        case (0x40<<4)|7: _FETCH();break;
        case (0x40<<4)|8: assert(false);break;
    /* EOR (d,x) */
        case (0x41<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x41<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0x41<<4)|2: _SA(c->PC);break;
        case (0x41<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0x41<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0x41<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x41<<4)|6: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x41<<4)|7: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x41<<4)|8: assert(false);break;
    /* WDM # */
        case (0x42<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x42<<4)|1: _FETCH();break;
        case (0x42<<4)|2: assert(false);break;
        case (0x42<<4)|3: assert(false);break;
        case (0x42<<4)|4: assert(false);break;
        case (0x42<<4)|5: assert(false);break;
        case (0x42<<4)|6: assert(false);break;
        case (0x42<<4)|7: assert(false);break;
        case (0x42<<4)|8: assert(false);break;
    /* EOR d,s */
        case (0x43<<4)|0: /* (unimpl) */;break;
        case (0x43<<4)|1: break;
        case (0x43<<4)|2: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x43<<4)|3: _B(c)^=_GD();_NZ16(_C(c));break;
        case (0x43<<4)|4: _FETCH();break;
        case (0x43<<4)|5: assert(false);break;
        case (0x43<<4)|6: assert(false);break;
        case (0x43<<4)|7: assert(false);break;
        case (0x43<<4)|8: assert(false);break;
    /* MVP xyc */
        case (0x44<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x44<<4)|1: _VPA();c->DBR=_GD();_SA(c->PC);break;
        case (0x44<<4)|2: _VDA(_GD());_SA(c->X--);break;
        case (0x44<<4)|3: _VDA(c->DBR);_SA(c->Y--);_WR();break;
        case (0x44<<4)|4: if(c->C){c->PC--;}break;
        case (0x44<<4)|5: c->C--?c->PC--:c->PC++;break;
        case (0x44<<4)|6: _FETCH();break;
        case (0x44<<4)|7: assert(false);break;
        case (0x44<<4)|8: assert(false);break;
    /* EOR d */
        case (0x45<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x45<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x45<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x45<<4)|3: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x45<<4)|4: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x45<<4)|5: assert(false);break;
        case (0x45<<4)|6: assert(false);break;
        case (0x45<<4)|7: assert(false);break;
        case (0x45<<4)|8: assert(false);break;
    /* LSR d */
        case (0x46<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x46<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x46<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x46<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x46<<4)|4: c->AD|=_GD()<<8;break;
        case (0x46<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_lsr(c,c->AD));}else{c->AD=_w65816_lsr16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x46<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x46<<4)|7: _FETCH();break;
        case (0x46<<4)|8: assert(false);break;
    /* EOR [d] */
        case (0x47<<4)|0: /* (unimpl) */;break;
        case (0x47<<4)|1: break;
        case (0x47<<4)|2: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x47<<4)|3: _B(c)^=_GD();_NZ16(_C(c));break;
        case (0x47<<4)|4: _FETCH();break;
        case (0x47<<4)|5: assert(false);break;
        case (0x47<<4)|6: assert(false);break;
        case (0x47<<4)|7: assert(false);break;
        case (0x47<<4)|8: assert(false);break;
    /* PHA s */
        case (0x48<<4)|0: _SA(c->PC);break;
        case (0x48<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),(_a8(c)?_A(c):_B(c)));_WR();break;
        case (0x48<<4)|2: if(_a8(c)){_FETCH();}else{_VDA(0);_SAD(_SP(_S(c)--),_A(c));_WR();}break;
        case (0x48<<4)|3: _FETCH();break;
        case (0x48<<4)|4: assert(false);break;
        case (0x48<<4)|5: assert(false);break;
        case (0x48<<4)|6: assert(false);break;
        case (0x48<<4)|7: assert(false);break;
        case (0x48<<4)|8: assert(false);break;
    /* EOR # */
        case (0x49<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x49<<4)|1: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0x49<<4)|2: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x49<<4)|3: assert(false);break;
        case (0x49<<4)|4: assert(false);break;
        case (0x49<<4)|5: assert(false);break;
        case (0x49<<4)|6: assert(false);break;
        case (0x49<<4)|7: assert(false);break;
        case (0x49<<4)|8: assert(false);break;
    /* LSR A */
        case (0x4A<<4)|0: _SA(c->PC);break;
        case (0x4A<<4)|1: if(_a8(c)){_A(c)=_w65816_lsr(c,_A(c));}else{_C(c)=_w65816_lsr16(c,_C(c));}_FETCH();break;
        case (0x4A<<4)|2: assert(false);break;
        case (0x4A<<4)|3: assert(false);break;
        case (0x4A<<4)|4: assert(false);break;
        case (0x4A<<4)|5: assert(false);break;
        case (0x4A<<4)|6: assert(false);break;
        case (0x4A<<4)|7: assert(false);break;
        case (0x4A<<4)|8: assert(false);break;
    /* PHK s */
        case (0x4B<<4)|0: _SA(c->PC);break;
        case (0x4B<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),c->PBR);_WR();break;
        case (0x4B<<4)|2: _FETCH();break;
        case (0x4B<<4)|3: assert(false);break;
        case (0x4B<<4)|4: assert(false);break;
        case (0x4B<<4)|5: assert(false);break;
        case (0x4B<<4)|6: assert(false);break;
        case (0x4B<<4)|7: assert(false);break;
        case (0x4B<<4)|8: assert(false);break;
    /* JMP a */
        case (0x4C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x4C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x4C<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);c->PC=_GA();_FETCH();break;
        case (0x4C<<4)|3: assert(false);break;
        case (0x4C<<4)|4: assert(false);break;
        case (0x4C<<4)|5: assert(false);break;
        case (0x4C<<4)|6: assert(false);break;
        case (0x4C<<4)|7: assert(false);break;
        case (0x4C<<4)|8: assert(false);break;
    /* EOR a */
        case (0x4D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x4D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x4D<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x4D<<4)|3: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x4D<<4)|4: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x4D<<4)|5: assert(false);break;
        case (0x4D<<4)|6: assert(false);break;
        case (0x4D<<4)|7: assert(false);break;
        case (0x4D<<4)|8: assert(false);break;
    /* LSR a */
        case (0x4E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x4E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x4E<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x4E<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x4E<<4)|4: c->AD|=_GD()<<8;break;
        case (0x4E<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_lsr(c,c->AD));}else{c->AD=_w65816_lsr16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x4E<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x4E<<4)|7: _FETCH();break;
        case (0x4E<<4)|8: assert(false);break;
    /* EOR al */
        case (0x4F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x4F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x4F<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0x4F<<4)|3: _VDA(_GD());_SA(c->AD);break;
        case (0x4F<<4)|4: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x4F<<4)|5: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x4F<<4)|6: assert(false);break;
        case (0x4F<<4)|7: assert(false);break;
        case (0x4F<<4)|8: assert(false);break;
    /* BVC r */
        case (0x50<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x50<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x40)!=0x0){_FETCH();};break;
        case (0x50<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x50<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0x50<<4)|4: assert(false);break;
        case (0x50<<4)|5: assert(false);break;
        case (0x50<<4)|6: assert(false);break;
        case (0x50<<4)|7: assert(false);break;
        case (0x50<<4)|8: assert(false);break;
    /* EOR (d),y */
        case (0x51<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x51<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0x51<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0x51<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x51<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x51<<4)|5: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x51<<4)|6: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x51<<4)|7: assert(false);break;
        case (0x51<<4)|8: assert(false);break;
    /* EOR (d) */
        case (0x52<<4)|0: /* (unimpl) */;break;
        case (0x52<<4)|1: break;
        case (0x52<<4)|2: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x52<<4)|3: _B(c)^=_GD();_NZ16(_C(c));break;
        case (0x52<<4)|4: _FETCH();break;
        case (0x52<<4)|5: assert(false);break;
        case (0x52<<4)|6: assert(false);break;
        case (0x52<<4)|7: assert(false);break;
        case (0x52<<4)|8: assert(false);break;
    /* EOR (d,s),y */
        case (0x53<<4)|0: /* (unimpl) */;break;
        case (0x53<<4)|1: break;
        case (0x53<<4)|2: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x53<<4)|3: _B(c)^=_GD();_NZ16(_C(c));break;
        case (0x53<<4)|4: _FETCH();break;
        case (0x53<<4)|5: assert(false);break;
        case (0x53<<4)|6: assert(false);break;
        case (0x53<<4)|7: assert(false);break;
        case (0x53<<4)|8: assert(false);break;
    /* MVN xyc */
        case (0x54<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x54<<4)|1: _VPA();c->DBR=_GD();_SA(c->PC);break;
        case (0x54<<4)|2: _VDA(_GD());_SA(c->X++);break;
        case (0x54<<4)|3: _VDA(c->DBR);_SA(c->Y++);_WR();break;
        case (0x54<<4)|4: if(c->C){c->PC--;}break;
        case (0x54<<4)|5: c->C--?c->PC--:c->PC++;break;
        case (0x54<<4)|6: _FETCH();break;
        case (0x54<<4)|7: assert(false);break;
        case (0x54<<4)|8: assert(false);break;
    /* EOR d,x */
        case (0x55<<4)|0: _VPA();_SA(c->PC);break;
        case (0x55<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x55<<4)|2: _SA(c->PC++);break;
        case (0x55<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x55<<4)|4: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x55<<4)|5: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x55<<4)|6: assert(false);break;
        case (0x55<<4)|7: assert(false);break;
        case (0x55<<4)|8: assert(false);break;
    /* LSR d,x */
        case (0x56<<4)|0: _VPA();_SA(c->PC);break;
        case (0x56<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x56<<4)|2: _SA(c->PC++);break;
        case (0x56<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x56<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x56<<4)|5: c->AD|=_GD()<<8;break;
        case (0x56<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_lsr(c,c->AD));}else{c->AD=_w65816_lsr16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x56<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x56<<4)|8: _FETCH();break;
    /* EOR [d],y */
        case (0x57<<4)|0: /* (unimpl) */;break;
        case (0x57<<4)|1: break;
        case (0x57<<4)|2: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x57<<4)|3: _B(c)^=_GD();_NZ16(_C(c));break;
        case (0x57<<4)|4: _FETCH();break;
        case (0x57<<4)|5: assert(false);break;
        case (0x57<<4)|6: assert(false);break;
        case (0x57<<4)|7: assert(false);break;
        case (0x57<<4)|8: assert(false);break;
    /* CLI i */
        case (0x58<<4)|0: _SA(c->PC);break;
        case (0x58<<4)|1: c->P&=~0x4;_FETCH();break;
        case (0x58<<4)|2: assert(false);break;
        case (0x58<<4)|3: assert(false);break;
        case (0x58<<4)|4: assert(false);break;
        case (0x58<<4)|5: assert(false);break;
        case (0x58<<4)|6: assert(false);break;
        case (0x58<<4)|7: assert(false);break;
        case (0x58<<4)|8: assert(false);break;
    /* EOR a,y */
        case (0x59<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x59<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x59<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x59<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x59<<4)|4: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x59<<4)|5: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x59<<4)|6: assert(false);break;
        case (0x59<<4)|7: assert(false);break;
        case (0x59<<4)|8: assert(false);break;
    /* PHY s */
        case (0x5A<<4)|0: _SA(c->PC);break;
        case (0x5A<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),(_i8(c)?_YL(c):_YH(c)));_WR();break;
        case (0x5A<<4)|2: if(_i8(c)){_FETCH();}else{_VDA(0);_SAD(_SP(_S(c)--),_YL(c));_WR();}break;
        case (0x5A<<4)|3: _FETCH();break;
        case (0x5A<<4)|4: assert(false);break;
        case (0x5A<<4)|5: assert(false);break;
        case (0x5A<<4)|6: assert(false);break;
        case (0x5A<<4)|7: assert(false);break;
        case (0x5A<<4)|8: assert(false);break;
    /* TCD i */
        case (0x5B<<4)|0: _SA(c->PC);break;
        case (0x5B<<4)|1: c->D=c->C;_NZ16(c->D);_FETCH();break;
        case (0x5B<<4)|2: assert(false);break;
        case (0x5B<<4)|3: assert(false);break;
        case (0x5B<<4)|4: assert(false);break;
        case (0x5B<<4)|5: assert(false);break;
        case (0x5B<<4)|6: assert(false);break;
        case (0x5B<<4)|7: assert(false);break;
        case (0x5B<<4)|8: assert(false);break;
    /* JMP al */
        case (0x5C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x5C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x5C<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0x5C<<4)|3: _VDA(_GD());_SA(c->AD);c->PBR=_GB();c->PC=_GA();_FETCH();break;
        case (0x5C<<4)|4: assert(false);break;
        case (0x5C<<4)|5: assert(false);break;
        case (0x5C<<4)|6: assert(false);break;
        case (0x5C<<4)|7: assert(false);break;
        case (0x5C<<4)|8: assert(false);break;
    /* EOR a,x */
        case (0x5D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x5D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x5D<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x5D<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x5D<<4)|4: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x5D<<4)|5: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x5D<<4)|6: assert(false);break;
        case (0x5D<<4)|7: assert(false);break;
        case (0x5D<<4)|8: assert(false);break;
    /* LSR a,x */
        case (0x5E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x5E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x5E<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0x5E<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x5E<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x5E<<4)|5: c->AD|=_GD()<<8;break;
        case (0x5E<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_lsr(c,c->AD));}else{c->AD=_w65816_lsr16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x5E<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x5E<<4)|8: _FETCH();break;
    /* EOR al,x */
        case (0x5F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x5F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x5F<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0x5F<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));break;
        case (0x5F<<4)|4: _A(c)^=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x5F<<4)|5: _B(c)^=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x5F<<4)|6: assert(false);break;
        case (0x5F<<4)|7: assert(false);break;
        case (0x5F<<4)|8: assert(false);break;
    /* RTS s */
        case (0x60<<4)|0: _SA(c->PC);break;
        case (0x60<<4)|1: _SA(c->PC);break;
        case (0x60<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0x60<<4)|3: _VDA(0);_SA(_SP(++_S(c)));c->AD=_GD();break;
        case (0x60<<4)|4: c->PC=(_GD()<<8)|c->AD;_SA(_SP(_S(c)));break;
        case (0x60<<4)|5: _FETCH();break;
        case (0x60<<4)|6: assert(false);break;
        case (0x60<<4)|7: assert(false);break;
        case (0x60<<4)|8: assert(false);break;
    /* ADC (d,x) */
        case (0x61<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x61<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0x61<<4)|2: _SA(c->PC);break;
        case (0x61<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0x61<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0x61<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x61<<4)|6: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x61<<4)|7: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x61<<4)|8: assert(false);break;
    /* PER s (unimpl) */
        case (0x62<<4)|0: _SA(c->PC);break;
        case (0x62<<4)|1: break;
        case (0x62<<4)|2: _FETCH();break;
        case (0x62<<4)|3: assert(false);break;
        case (0x62<<4)|4: assert(false);break;
        case (0x62<<4)|5: assert(false);break;
        case (0x62<<4)|6: assert(false);break;
        case (0x62<<4)|7: assert(false);break;
        case (0x62<<4)|8: assert(false);break;
    /* ADC d,s */
        case (0x63<<4)|0: /* (unimpl) */;break;
        case (0x63<<4)|1: break;
        case (0x63<<4)|2: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x63<<4)|3: _w65816_adc16(c,c->AD|(_GD()<<8));break;
        case (0x63<<4)|4: _FETCH();break;
        case (0x63<<4)|5: assert(false);break;
        case (0x63<<4)|6: assert(false);break;
        case (0x63<<4)|7: assert(false);break;
        case (0x63<<4)|8: assert(false);break;
    /* STZ d */
        case (0x64<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x64<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x64<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);_SD(0);_WR();break;
        case (0x64<<4)|3: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,0);_WR();}break;
        case (0x64<<4)|4: _FETCH();break;
        case (0x64<<4)|5: assert(false);break;
        case (0x64<<4)|6: assert(false);break;
        case (0x64<<4)|7: assert(false);break;
        case (0x64<<4)|8: assert(false);break;
    /* ADC d */
        case (0x65<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x65<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x65<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x65<<4)|3: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x65<<4)|4: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x65<<4)|5: assert(false);break;
        case (0x65<<4)|6: assert(false);break;
        case (0x65<<4)|7: assert(false);break;
        case (0x65<<4)|8: assert(false);break;
    /* ROR d */
        case (0x66<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x66<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x66<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0x66<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x66<<4)|4: c->AD|=_GD()<<8;break;
        case (0x66<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_ror(c,c->AD));}else{c->AD=_w65816_ror16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x66<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x66<<4)|7: _FETCH();break;
        case (0x66<<4)|8: assert(false);break;
    /* ADC [d] */
        case (0x67<<4)|0: /* (unimpl) */;break;
        case (0x67<<4)|1: break;
        case (0x67<<4)|2: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x67<<4)|3: _w65816_adc16(c,c->AD|(_GD()<<8));break;
        case (0x67<<4)|4: _FETCH();break;
        case (0x67<<4)|5: assert(false);break;
        case (0x67<<4)|6: assert(false);break;
        case (0x67<<4)|7: assert(false);break;
        case (0x67<<4)|8: assert(false);break;
    /* PLA s */
        case (0x68<<4)|0: _SA(c->PC);break;
        case (0x68<<4)|1: _SA(c->PC);break;
        case (0x68<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0x68<<4)|3: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(0);_SA(_SP(++_S(c)));}break;
        case (0x68<<4)|4: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0x68<<4)|5: assert(false);break;
        case (0x68<<4)|6: assert(false);break;
        case (0x68<<4)|7: assert(false);break;
        case (0x68<<4)|8: assert(false);break;
    /* ADC # */
        case (0x69<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x69<<4)|1: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VPA();_SA(c->PC++);}break;
        case (0x69<<4)|2: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x69<<4)|3: assert(false);break;
        case (0x69<<4)|4: assert(false);break;
        case (0x69<<4)|5: assert(false);break;
        case (0x69<<4)|6: assert(false);break;
        case (0x69<<4)|7: assert(false);break;
        case (0x69<<4)|8: assert(false);break;
    /* ROR A */
        case (0x6A<<4)|0: _SA(c->PC);break;
        case (0x6A<<4)|1: if(_a8(c)){_A(c)=_w65816_ror(c,_A(c));}else{_C(c)=_w65816_ror16(c,_C(c));}_FETCH();break;
        case (0x6A<<4)|2: assert(false);break;
        case (0x6A<<4)|3: assert(false);break;
        case (0x6A<<4)|4: assert(false);break;
        case (0x6A<<4)|5: assert(false);break;
        case (0x6A<<4)|6: assert(false);break;
        case (0x6A<<4)|7: assert(false);break;
        case (0x6A<<4)|8: assert(false);break;
    /* RTL s */
        case (0x6B<<4)|0: _SA(c->PC);break;
        case (0x6B<<4)|1: _SA(c->PC);break;
        case (0x6B<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0x6B<<4)|3: _VDA(0);_SA(_SP(++_S(c)));c->AD=_GD();break;
        case (0x6B<<4)|4: _VDA(0);_SA(_SP(++_S(c)));c->PC=(_GD()<<8)|c->AD;break;
        case (0x6B<<4)|5: c->PBR=_GD();_FETCH();break;
        case (0x6B<<4)|6: assert(false);break;
        case (0x6B<<4)|7: assert(false);break;
        case (0x6B<<4)|8: assert(false);break;
    /* JMP (a) */
        case (0x6C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x6C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x6C<<4)|2: _VDA(_GB());c->AD|=_GD()<<8;_SA(c->AD);break;
        case (0x6C<<4)|3: _VDA(_GB());_SA(c->AD+1);c->AD=_GD();break;
        case (0x6C<<4)|4: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x6C<<4)|5: assert(false);break;
        case (0x6C<<4)|6: assert(false);break;
        case (0x6C<<4)|7: assert(false);break;
        case (0x6C<<4)|8: assert(false);break;
    /* ADC a */
        case (0x6D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x6D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x6D<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x6D<<4)|3: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x6D<<4)|4: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x6D<<4)|5: assert(false);break;
        case (0x6D<<4)|6: assert(false);break;
        case (0x6D<<4)|7: assert(false);break;
        case (0x6D<<4)|8: assert(false);break;
    /* ROR a */
        case (0x6E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x6E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x6E<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0x6E<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x6E<<4)|4: c->AD|=_GD()<<8;break;
        case (0x6E<<4)|5: _VDA(_GB());if(_a8(c)){_SD(_w65816_ror(c,c->AD));}else{c->AD=_w65816_ror16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x6E<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x6E<<4)|7: _FETCH();break;
        case (0x6E<<4)|8: assert(false);break;
    /* ADC al */
        case (0x6F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x6F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x6F<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0x6F<<4)|3: _VDA(_GD());_SA(c->AD);break;
        case (0x6F<<4)|4: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x6F<<4)|5: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x6F<<4)|6: assert(false);break;
        case (0x6F<<4)|7: assert(false);break;
        case (0x6F<<4)|8: assert(false);break;
    /* BVS r */
        case (0x70<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x70<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x40)!=0x40){_FETCH();};break;
        case (0x70<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x70<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0x70<<4)|4: assert(false);break;
        case (0x70<<4)|5: assert(false);break;
        case (0x70<<4)|6: assert(false);break;
        case (0x70<<4)|7: assert(false);break;
        case (0x70<<4)|8: assert(false);break;
    /* ADC (d),y */
        case (0x71<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x71<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0x71<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0x71<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x71<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x71<<4)|5: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x71<<4)|6: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x71<<4)|7: assert(false);break;
        case (0x71<<4)|8: assert(false);break;
    /* ADC (d) */
        case (0x72<<4)|0: /* (unimpl) */;break;
        case (0x72<<4)|1: break;
        case (0x72<<4)|2: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x72<<4)|3: _w65816_adc16(c,c->AD|(_GD()<<8));break;
        case (0x72<<4)|4: _FETCH();break;
        case (0x72<<4)|5: assert(false);break;
        case (0x72<<4)|6: assert(false);break;
        case (0x72<<4)|7: assert(false);break;
        case (0x72<<4)|8: assert(false);break;
    /* ADC (d,s),y */
        case (0x73<<4)|0: /* (unimpl) */;break;
        case (0x73<<4)|1: break;
        case (0x73<<4)|2: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x73<<4)|3: _w65816_adc16(c,c->AD|(_GD()<<8));break;
        case (0x73<<4)|4: _FETCH();break;
        case (0x73<<4)|5: assert(false);break;
        case (0x73<<4)|6: assert(false);break;
        case (0x73<<4)|7: assert(false);break;
        case (0x73<<4)|8: assert(false);break;
    /* STZ d,x */
        case (0x74<<4)|0: _VPA();_SA(c->PC);break;
        case (0x74<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x74<<4)|2: _SA(c->PC++);break;
        case (0x74<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));_SD(0);_WR();break;
        case (0x74<<4)|4: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,0);_WR();}break;
        case (0x74<<4)|5: _FETCH();break;
        case (0x74<<4)|6: assert(false);break;
        case (0x74<<4)|7: assert(false);break;
        case (0x74<<4)|8: assert(false);break;
    /* ADC d,x */
        case (0x75<<4)|0: _VPA();_SA(c->PC);break;
        case (0x75<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x75<<4)|2: _SA(c->PC++);break;
        case (0x75<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x75<<4)|4: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x75<<4)|5: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x75<<4)|6: assert(false);break;
        case (0x75<<4)|7: assert(false);break;
        case (0x75<<4)|8: assert(false);break;
    /* ROR d,x */
        case (0x76<<4)|0: _VPA();_SA(c->PC);break;
        case (0x76<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x76<<4)|2: _SA(c->PC++);break;
        case (0x76<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0x76<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x76<<4)|5: c->AD|=_GD()<<8;break;
        case (0x76<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_ror(c,c->AD));}else{c->AD=_w65816_ror16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x76<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x76<<4)|8: _FETCH();break;
    /* ADC [d],y */
        case (0x77<<4)|0: /* (unimpl) */;break;
        case (0x77<<4)|1: break;
        case (0x77<<4)|2: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x77<<4)|3: _w65816_adc16(c,c->AD|(_GD()<<8));break;
        case (0x77<<4)|4: _FETCH();break;
        case (0x77<<4)|5: assert(false);break;
        case (0x77<<4)|6: assert(false);break;
        case (0x77<<4)|7: assert(false);break;
        case (0x77<<4)|8: assert(false);break;
    /* SEI i */
        case (0x78<<4)|0: _SA(c->PC);break;
        case (0x78<<4)|1: c->P|=0x4;_FETCH();break;
        case (0x78<<4)|2: assert(false);break;
        case (0x78<<4)|3: assert(false);break;
        case (0x78<<4)|4: assert(false);break;
        case (0x78<<4)|5: assert(false);break;
        case (0x78<<4)|6: assert(false);break;
        case (0x78<<4)|7: assert(false);break;
        case (0x78<<4)|8: assert(false);break;
    /* ADC a,y */
        case (0x79<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x79<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x79<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0x79<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0x79<<4)|4: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x79<<4)|5: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x79<<4)|6: assert(false);break;
        case (0x79<<4)|7: assert(false);break;
        case (0x79<<4)|8: assert(false);break;
    /* PLY s */
        case (0x7A<<4)|0: _SA(c->PC);break;
        case (0x7A<<4)|1: _SA(c->PC);break;
        case (0x7A<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0x7A<<4)|3: _YL(c)=_GD();if(_i8(c)){_NZ(_YL(c));_FETCH();}else{_VDA(0);_SA(_SP(++_S(c)));}break;
        case (0x7A<<4)|4: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0x7A<<4)|5: assert(false);break;
        case (0x7A<<4)|6: assert(false);break;
        case (0x7A<<4)|7: assert(false);break;
        case (0x7A<<4)|8: assert(false);break;
    /* TDC i */
        case (0x7B<<4)|0: _SA(c->PC);break;
        case (0x7B<<4)|1: c->C=c->D;_NZ(c->C);_FETCH();break;
        case (0x7B<<4)|2: assert(false);break;
        case (0x7B<<4)|3: assert(false);break;
        case (0x7B<<4)|4: assert(false);break;
        case (0x7B<<4)|5: assert(false);break;
        case (0x7B<<4)|6: assert(false);break;
        case (0x7B<<4)|7: assert(false);break;
        case (0x7B<<4)|8: assert(false);break;
    /* JMP (a,x) */
        case (0x7C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x7C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x7C<<4)|2: _SA(c->PC);c->AD=(_GD()<<8)|c->AD;break;
        case (0x7C<<4)|3: _VDA(c->DBR);c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0x7C<<4)|4: _VDA(c->DBR);_SA(c->AD+_X(c)+1);c->AD=_GD();break;
        case (0x7C<<4)|5: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0x7C<<4)|6: assert(false);break;
        case (0x7C<<4)|7: assert(false);break;
        case (0x7C<<4)|8: assert(false);break;
    /* ADC a,x */
        case (0x7D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x7D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x7D<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0x7D<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x7D<<4)|4: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x7D<<4)|5: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x7D<<4)|6: assert(false);break;
        case (0x7D<<4)|7: assert(false);break;
        case (0x7D<<4)|8: assert(false);break;
    /* ROR a,x */
        case (0x7E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x7E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x7E<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0x7E<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0x7E<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x7E<<4)|5: c->AD|=_GD()<<8;break;
        case (0x7E<<4)|6: _VDA(_GB());if(_a8(c)){_SD(_w65816_ror(c,c->AD));}else{c->AD=_w65816_ror16(c,c->AD);_SD(c->AD>>8);}_WR();break;
        case (0x7E<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0x7E<<4)|8: _FETCH();break;
    /* ADC al,x */
        case (0x7F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x7F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x7F<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0x7F<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));break;
        case (0x7F<<4)|4: if(_a8(c)){_w65816_adc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0x7F<<4)|5: _w65816_adc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x7F<<4)|6: assert(false);break;
        case (0x7F<<4)|7: assert(false);break;
        case (0x7F<<4)|8: assert(false);break;
    /* BRA r */
        case (0x80<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x80<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();break;
        case (0x80<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x80<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0x80<<4)|4: assert(false);break;
        case (0x80<<4)|5: assert(false);break;
        case (0x80<<4)|6: assert(false);break;
        case (0x80<<4)|7: assert(false);break;
        case (0x80<<4)|8: assert(false);break;
    /* STA (d,x) */
        case (0x81<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x81<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0x81<<4)|2: _SA(c->PC);break;
        case (0x81<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0x81<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0x81<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);_SD(_A(c));_WR();break;
        case (0x81<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x81<<4)|7: _FETCH();break;
        case (0x81<<4)|8: assert(false);break;
    /* BRL rl */
        case (0x82<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x82<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x82<<4)|2: _SA(c->PC);c->AD=(_GD()<<8)|c->AD;break;
        case (0x82<<4)|3: c->PC+=c->AD;_FETCH();break;
        case (0x82<<4)|4: assert(false);break;
        case (0x82<<4)|5: assert(false);break;
        case (0x82<<4)|6: assert(false);break;
        case (0x82<<4)|7: assert(false);break;
        case (0x82<<4)|8: assert(false);break;
    /* STA d,s */
        case (0x83<<4)|0: /* (unimpl) */;break;
        case (0x83<<4)|1: _SD(_A(c));_WR();break;
        case (0x83<<4)|2: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x83<<4)|3: _FETCH();break;
        case (0x83<<4)|4: assert(false);break;
        case (0x83<<4)|5: assert(false);break;
        case (0x83<<4)|6: assert(false);break;
        case (0x83<<4)|7: assert(false);break;
        case (0x83<<4)|8: assert(false);break;
    /* STY d */
        case (0x84<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x84<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x84<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);_SD(_YL(c));_WR();break;
        case (0x84<<4)|3: if(_i8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_YH(c));_WR();}break;
        case (0x84<<4)|4: _FETCH();break;
        case (0x84<<4)|5: assert(false);break;
        case (0x84<<4)|6: assert(false);break;
        case (0x84<<4)|7: assert(false);break;
        case (0x84<<4)|8: assert(false);break;
    /* STA d */
        case (0x85<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x85<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x85<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);_SD(_A(c));_WR();break;
        case (0x85<<4)|3: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x85<<4)|4: _FETCH();break;
        case (0x85<<4)|5: assert(false);break;
        case (0x85<<4)|6: assert(false);break;
        case (0x85<<4)|7: assert(false);break;
        case (0x85<<4)|8: assert(false);break;
    /* STX d */
        case (0x86<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x86<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0x86<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);_SD(_XL(c));_WR();break;
        case (0x86<<4)|3: if(_i8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_XH(c));_WR();}break;
        case (0x86<<4)|4: _FETCH();break;
        case (0x86<<4)|5: assert(false);break;
        case (0x86<<4)|6: assert(false);break;
        case (0x86<<4)|7: assert(false);break;
        case (0x86<<4)|8: assert(false);break;
    /* STA [d] */
        case (0x87<<4)|0: /* (unimpl) */;break;
        case (0x87<<4)|1: _SD(_A(c));_WR();break;
        case (0x87<<4)|2: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x87<<4)|3: _FETCH();break;
        case (0x87<<4)|4: assert(false);break;
        case (0x87<<4)|5: assert(false);break;
        case (0x87<<4)|6: assert(false);break;
        case (0x87<<4)|7: assert(false);break;
        case (0x87<<4)|8: assert(false);break;
    /* DEY i */
        case (0x88<<4)|0: _SA(c->PC);break;
        case (0x88<<4)|1: if(_i8(c)){_YL(c)--;_NZ(_YL(c));}else{_Y(c)--;_NZ16(_Y(c));}_FETCH();break;
        case (0x88<<4)|2: assert(false);break;
        case (0x88<<4)|3: assert(false);break;
        case (0x88<<4)|4: assert(false);break;
        case (0x88<<4)|5: assert(false);break;
        case (0x88<<4)|6: assert(false);break;
        case (0x88<<4)|7: assert(false);break;
        case (0x88<<4)|8: assert(false);break;
    /* BIT # */
        case (0x89<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x89<<4)|1: if(_a8(c)){_w65816_bit(c,_GD());_FETCH();}else{c->AD=_GD();_VPA();_SA(c->PC++);}break;
        case (0x89<<4)|2: _w65816_bit16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0x89<<4)|3: assert(false);break;
        case (0x89<<4)|4: assert(false);break;
        case (0x89<<4)|5: assert(false);break;
        case (0x89<<4)|6: assert(false);break;
        case (0x89<<4)|7: assert(false);break;
        case (0x89<<4)|8: assert(false);break;
    /* TXA i */
        case (0x8A<<4)|0: _SA(c->PC);break;
        case (0x8A<<4)|1: if(_a8(c)){_A(c)=_XL(c);_NZ(_A(c));}else{_C(c)=_X(c);_NZ16(_C(c));}_FETCH();break;
        case (0x8A<<4)|2: assert(false);break;
        case (0x8A<<4)|3: assert(false);break;
        case (0x8A<<4)|4: assert(false);break;
        case (0x8A<<4)|5: assert(false);break;
        case (0x8A<<4)|6: assert(false);break;
        case (0x8A<<4)|7: assert(false);break;
        case (0x8A<<4)|8: assert(false);break;
    /* PHB s */
        case (0x8B<<4)|0: _SA(c->PC);break;
        case (0x8B<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),c->DBR);_WR();break;
        case (0x8B<<4)|2: _FETCH();break;
        case (0x8B<<4)|3: assert(false);break;
        case (0x8B<<4)|4: assert(false);break;
        case (0x8B<<4)|5: assert(false);break;
        case (0x8B<<4)|6: assert(false);break;
        case (0x8B<<4)|7: assert(false);break;
        case (0x8B<<4)|8: assert(false);break;
    /* STY a */
        case (0x8C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x8C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x8C<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);_SD(_YL(c));_WR();break;
        case (0x8C<<4)|3: if(_i8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_YH(c));_WR();}break;
        case (0x8C<<4)|4: _FETCH();break;
        case (0x8C<<4)|5: assert(false);break;
        case (0x8C<<4)|6: assert(false);break;
        case (0x8C<<4)|7: assert(false);break;
        case (0x8C<<4)|8: assert(false);break;
    /* STA a */
        case (0x8D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x8D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x8D<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);_SD(_A(c));_WR();break;
        case (0x8D<<4)|3: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x8D<<4)|4: _FETCH();break;
        case (0x8D<<4)|5: assert(false);break;
        case (0x8D<<4)|6: assert(false);break;
        case (0x8D<<4)|7: assert(false);break;
        case (0x8D<<4)|8: assert(false);break;
    /* STX a */
        case (0x8E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x8E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x8E<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);_SD(_XL(c));_WR();break;
        case (0x8E<<4)|3: if(_i8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_XH(c));_WR();}break;
        case (0x8E<<4)|4: _FETCH();break;
        case (0x8E<<4)|5: assert(false);break;
        case (0x8E<<4)|6: assert(false);break;
        case (0x8E<<4)|7: assert(false);break;
        case (0x8E<<4)|8: assert(false);break;
    /* STA al */
        case (0x8F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x8F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x8F<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0x8F<<4)|3: _VDA(_GD());_SA(c->AD);_SD(_A(c));_WR();break;
        case (0x8F<<4)|4: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x8F<<4)|5: _FETCH();break;
        case (0x8F<<4)|6: assert(false);break;
        case (0x8F<<4)|7: assert(false);break;
        case (0x8F<<4)|8: assert(false);break;
    /* BCC r */
        case (0x90<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x90<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x1)!=0x0){_FETCH();};break;
        case (0x90<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0x90<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0x90<<4)|4: assert(false);break;
        case (0x90<<4)|5: assert(false);break;
        case (0x90<<4)|6: assert(false);break;
        case (0x90<<4)|7: assert(false);break;
        case (0x90<<4)|8: assert(false);break;
    /* STA (d),y */
        case (0x91<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x91<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0x91<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0x91<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));break;
        case (0x91<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));_SD(_A(c));_WR();break;
        case (0x91<<4)|5: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x91<<4)|6: _FETCH();break;
        case (0x91<<4)|7: assert(false);break;
        case (0x91<<4)|8: assert(false);break;
    /* STA (d) */
        case (0x92<<4)|0: /* (unimpl) */;break;
        case (0x92<<4)|1: _SD(_A(c));_WR();break;
        case (0x92<<4)|2: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x92<<4)|3: _FETCH();break;
        case (0x92<<4)|4: assert(false);break;
        case (0x92<<4)|5: assert(false);break;
        case (0x92<<4)|6: assert(false);break;
        case (0x92<<4)|7: assert(false);break;
        case (0x92<<4)|8: assert(false);break;
    /* STA (d,s),y */
        case (0x93<<4)|0: /* (unimpl) */;break;
        case (0x93<<4)|1: _SD(_A(c));_WR();break;
        case (0x93<<4)|2: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x93<<4)|3: _FETCH();break;
        case (0x93<<4)|4: assert(false);break;
        case (0x93<<4)|5: assert(false);break;
        case (0x93<<4)|6: assert(false);break;
        case (0x93<<4)|7: assert(false);break;
        case (0x93<<4)|8: assert(false);break;
    /* STY d,x */
        case (0x94<<4)|0: _VPA();_SA(c->PC);break;
        case (0x94<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x94<<4)|2: _SA(c->PC++);break;
        case (0x94<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));_SD(_YL(c));_WR();break;
        case (0x94<<4)|4: if(_i8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_YH(c));_WR();}break;
        case (0x94<<4)|5: _FETCH();break;
        case (0x94<<4)|6: assert(false);break;
        case (0x94<<4)|7: assert(false);break;
        case (0x94<<4)|8: assert(false);break;
    /* STA d,x */
        case (0x95<<4)|0: _VPA();_SA(c->PC);break;
        case (0x95<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0x95<<4)|2: _SA(c->PC++);break;
        case (0x95<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));_SD(_A(c));_WR();break;
        case (0x95<<4)|4: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x95<<4)|5: _FETCH();break;
        case (0x95<<4)|6: assert(false);break;
        case (0x95<<4)|7: assert(false);break;
        case (0x95<<4)|8: assert(false);break;
    /* STX d,y */
        case (0x96<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x96<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0x96<<4)|2: _SA(c->PC);break;
        case (0x96<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_Y(c))&0xFF):(c->D+c->AD+_Y(c)));_SD(_XL(c));_WR();break;
        case (0x96<<4)|4: if(_i8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_XH(c));_WR();}break;
        case (0x96<<4)|5: _FETCH();break;
        case (0x96<<4)|6: assert(false);break;
        case (0x96<<4)|7: assert(false);break;
        case (0x96<<4)|8: assert(false);break;
    /* STA [d],y */
        case (0x97<<4)|0: /* (unimpl) */;break;
        case (0x97<<4)|1: _SD(_A(c));_WR();break;
        case (0x97<<4)|2: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x97<<4)|3: _FETCH();break;
        case (0x97<<4)|4: assert(false);break;
        case (0x97<<4)|5: assert(false);break;
        case (0x97<<4)|6: assert(false);break;
        case (0x97<<4)|7: assert(false);break;
        case (0x97<<4)|8: assert(false);break;
    /* TYA i */
        case (0x98<<4)|0: _SA(c->PC);break;
        case (0x98<<4)|1: if(_a8(c)){_A(c)=_YL(c);_NZ(_A(c));}else{_C(c)=_Y(c);_NZ16(_C(c));}_FETCH();break;
        case (0x98<<4)|2: assert(false);break;
        case (0x98<<4)|3: assert(false);break;
        case (0x98<<4)|4: assert(false);break;
        case (0x98<<4)|5: assert(false);break;
        case (0x98<<4)|6: assert(false);break;
        case (0x98<<4)|7: assert(false);break;
        case (0x98<<4)|8: assert(false);break;
    /* STA a,y */
        case (0x99<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x99<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x99<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));break;
        case (0x99<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));_SD(_A(c));_WR();break;
        case (0x99<<4)|4: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x99<<4)|5: _FETCH();break;
        case (0x99<<4)|6: assert(false);break;
        case (0x99<<4)|7: assert(false);break;
        case (0x99<<4)|8: assert(false);break;
    /* TXS i */
        case (0x9A<<4)|0: _SA(c->PC);break;
        case (0x9A<<4)|1: _S(c)=_X(c);_FETCH();break;
        case (0x9A<<4)|2: assert(false);break;
        case (0x9A<<4)|3: assert(false);break;
        case (0x9A<<4)|4: assert(false);break;
        case (0x9A<<4)|5: assert(false);break;
        case (0x9A<<4)|6: assert(false);break;
        case (0x9A<<4)|7: assert(false);break;
        case (0x9A<<4)|8: assert(false);break;
    /* TXY i */
        case (0x9B<<4)|0: _SA(c->PC);break;
        case (0x9B<<4)|1: if(_i8(c)){_YL(c)=_XL(c);_NZ(_YL(c));}else{_Y(c)=_X(c);_NZ16(_Y(c));}_FETCH();break;
        case (0x9B<<4)|2: assert(false);break;
        case (0x9B<<4)|3: assert(false);break;
        case (0x9B<<4)|4: assert(false);break;
        case (0x9B<<4)|5: assert(false);break;
        case (0x9B<<4)|6: assert(false);break;
        case (0x9B<<4)|7: assert(false);break;
        case (0x9B<<4)|8: assert(false);break;
    /* STZ a */
        case (0x9C<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x9C<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x9C<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);_SD(0);_WR();break;
        case (0x9C<<4)|3: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,0);_WR();}break;
        case (0x9C<<4)|4: _FETCH();break;
        case (0x9C<<4)|5: assert(false);break;
        case (0x9C<<4)|6: assert(false);break;
        case (0x9C<<4)|7: assert(false);break;
        case (0x9C<<4)|8: assert(false);break;
    /* STA a,x */
        case (0x9D<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x9D<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x9D<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0x9D<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));_SD(_A(c));_WR();break;
        case (0x9D<<4)|4: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x9D<<4)|5: _FETCH();break;
        case (0x9D<<4)|6: assert(false);break;
        case (0x9D<<4)|7: assert(false);break;
        case (0x9D<<4)|8: assert(false);break;
    /* STZ a,x */
        case (0x9E<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x9E<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x9E<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0x9E<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));_SD(0);_WR();break;
        case (0x9E<<4)|4: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,0);_WR();}break;
        case (0x9E<<4)|5: _FETCH();break;
        case (0x9E<<4)|6: assert(false);break;
        case (0x9E<<4)|7: assert(false);break;
        case (0x9E<<4)|8: assert(false);break;
    /* STA al,x */
        case (0x9F<<4)|0: _VPA();_SA(c->PC++);break;
        case (0x9F<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0x9F<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0x9F<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));_SD(_A(c));_WR();break;
        case (0x9F<<4)|4: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()+1,_B(c));_WR();}break;
        case (0x9F<<4)|5: _FETCH();break;
        case (0x9F<<4)|6: assert(false);break;
        case (0x9F<<4)|7: assert(false);break;
        case (0x9F<<4)|8: assert(false);break;
    /* LDY # */
        case (0xA0<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xA0<<4)|1: _YL(c)=_GD();if(_i8(c)){_NZ(_YL(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0xA0<<4)|2: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xA0<<4)|3: assert(false);break;
        case (0xA0<<4)|4: assert(false);break;
        case (0xA0<<4)|5: assert(false);break;
        case (0xA0<<4)|6: assert(false);break;
        case (0xA0<<4)|7: assert(false);break;
        case (0xA0<<4)|8: assert(false);break;
    /* LDA (d,x) */
        case (0xA1<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xA1<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0xA1<<4)|2: _SA(c->PC);break;
        case (0xA1<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0xA1<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0xA1<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xA1<<4)|6: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xA1<<4)|7: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA1<<4)|8: assert(false);break;
    /* LDX # */
        case (0xA2<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xA2<<4)|1: _XL(c)=_GD();if(_i8(c)){_NZ(_XL(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0xA2<<4)|2: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xA2<<4)|3: assert(false);break;
        case (0xA2<<4)|4: assert(false);break;
        case (0xA2<<4)|5: assert(false);break;
        case (0xA2<<4)|6: assert(false);break;
        case (0xA2<<4)|7: assert(false);break;
        case (0xA2<<4)|8: assert(false);break;
    /* LDA d,s */
        case (0xA3<<4)|0: /* (unimpl) */;break;
        case (0xA3<<4)|1: break;
        case (0xA3<<4)|2: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xA3<<4)|3: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA3<<4)|4: assert(false);break;
        case (0xA3<<4)|5: assert(false);break;
        case (0xA3<<4)|6: assert(false);break;
        case (0xA3<<4)|7: assert(false);break;
        case (0xA3<<4)|8: assert(false);break;
    /* LDY d */
        case (0xA4<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xA4<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xA4<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xA4<<4)|3: _YL(c)=_GD();if(_i8(c)){_NZ(_YL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xA4<<4)|4: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xA4<<4)|5: assert(false);break;
        case (0xA4<<4)|6: assert(false);break;
        case (0xA4<<4)|7: assert(false);break;
        case (0xA4<<4)|8: assert(false);break;
    /* LDA d */
        case (0xA5<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xA5<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xA5<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xA5<<4)|3: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xA5<<4)|4: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA5<<4)|5: assert(false);break;
        case (0xA5<<4)|6: assert(false);break;
        case (0xA5<<4)|7: assert(false);break;
        case (0xA5<<4)|8: assert(false);break;
    /* LDX d */
        case (0xA6<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xA6<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xA6<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xA6<<4)|3: _XL(c)=_GD();if(_i8(c)){_NZ(_XL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xA6<<4)|4: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xA6<<4)|5: assert(false);break;
        case (0xA6<<4)|6: assert(false);break;
        case (0xA6<<4)|7: assert(false);break;
        case (0xA6<<4)|8: assert(false);break;
    /* LDA [d] */
        case (0xA7<<4)|0: /* (unimpl) */;break;
        case (0xA7<<4)|1: break;
        case (0xA7<<4)|2: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xA7<<4)|3: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA7<<4)|4: assert(false);break;
        case (0xA7<<4)|5: assert(false);break;
        case (0xA7<<4)|6: assert(false);break;
        case (0xA7<<4)|7: assert(false);break;
        case (0xA7<<4)|8: assert(false);break;
    /* TAY i */
        case (0xA8<<4)|0: _SA(c->PC);break;
        case (0xA8<<4)|1: if(_i8(c)){_YL(c)=_A(c);_NZ(_YL(c));}else{_Y(c)=_C(c);_NZ16(_Y(c));}_FETCH();break;
        case (0xA8<<4)|2: assert(false);break;
        case (0xA8<<4)|3: assert(false);break;
        case (0xA8<<4)|4: assert(false);break;
        case (0xA8<<4)|5: assert(false);break;
        case (0xA8<<4)|6: assert(false);break;
        case (0xA8<<4)|7: assert(false);break;
        case (0xA8<<4)|8: assert(false);break;
    /* LDA # */
        case (0xA9<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xA9<<4)|1: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VPA();_SA(c->PC++);}break;
        case (0xA9<<4)|2: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xA9<<4)|3: assert(false);break;
        case (0xA9<<4)|4: assert(false);break;
        case (0xA9<<4)|5: assert(false);break;
        case (0xA9<<4)|6: assert(false);break;
        case (0xA9<<4)|7: assert(false);break;
        case (0xA9<<4)|8: assert(false);break;
    /* TAX i */
        case (0xAA<<4)|0: _SA(c->PC);break;
        case (0xAA<<4)|1: if(_i8(c)){_XL(c)=_A(c);_NZ(_XL(c));}else{_X(c)=_C(c);_NZ16(_X(c));}_FETCH();break;
        case (0xAA<<4)|2: assert(false);break;
        case (0xAA<<4)|3: assert(false);break;
        case (0xAA<<4)|4: assert(false);break;
        case (0xAA<<4)|5: assert(false);break;
        case (0xAA<<4)|6: assert(false);break;
        case (0xAA<<4)|7: assert(false);break;
        case (0xAA<<4)|8: assert(false);break;
    /* PLB s */
        case (0xAB<<4)|0: _SA(c->PC);break;
        case (0xAB<<4)|1: _SA(c->PC);break;
        case (0xAB<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0xAB<<4)|3: c->DBR=_GD();_NZ(c->DBR);_FETCH();break;
        case (0xAB<<4)|4: assert(false);break;
        case (0xAB<<4)|5: assert(false);break;
        case (0xAB<<4)|6: assert(false);break;
        case (0xAB<<4)|7: assert(false);break;
        case (0xAB<<4)|8: assert(false);break;
    /* LDY a */
        case (0xAC<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xAC<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xAC<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xAC<<4)|3: _YL(c)=_GD();if(_i8(c)){_NZ(_YL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xAC<<4)|4: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xAC<<4)|5: assert(false);break;
        case (0xAC<<4)|6: assert(false);break;
        case (0xAC<<4)|7: assert(false);break;
        case (0xAC<<4)|8: assert(false);break;
    /* LDA a */
        case (0xAD<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xAD<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xAD<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xAD<<4)|3: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xAD<<4)|4: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xAD<<4)|5: assert(false);break;
        case (0xAD<<4)|6: assert(false);break;
        case (0xAD<<4)|7: assert(false);break;
        case (0xAD<<4)|8: assert(false);break;
    /* LDX a */
        case (0xAE<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xAE<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xAE<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xAE<<4)|3: _XL(c)=_GD();if(_i8(c)){_NZ(_XL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xAE<<4)|4: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xAE<<4)|5: assert(false);break;
        case (0xAE<<4)|6: assert(false);break;
        case (0xAE<<4)|7: assert(false);break;
        case (0xAE<<4)|8: assert(false);break;
    /* LDA al */
        case (0xAF<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xAF<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xAF<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0xAF<<4)|3: _VDA(_GD());_SA(c->AD);break;
        case (0xAF<<4)|4: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xAF<<4)|5: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xAF<<4)|6: assert(false);break;
        case (0xAF<<4)|7: assert(false);break;
        case (0xAF<<4)|8: assert(false);break;
    /* BCS r */
        case (0xB0<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xB0<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x1)!=0x1){_FETCH();};break;
        case (0xB0<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0xB0<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0xB0<<4)|4: assert(false);break;
        case (0xB0<<4)|5: assert(false);break;
        case (0xB0<<4)|6: assert(false);break;
        case (0xB0<<4)|7: assert(false);break;
        case (0xB0<<4)|8: assert(false);break;
    /* LDA (d),y */
        case (0xB1<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xB1<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0xB1<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0xB1<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xB1<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0xB1<<4)|5: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB1<<4)|6: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB1<<4)|7: assert(false);break;
        case (0xB1<<4)|8: assert(false);break;
    /* LDA (d) */
        case (0xB2<<4)|0: /* (unimpl) */;break;
        case (0xB2<<4)|1: break;
        case (0xB2<<4)|2: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB2<<4)|3: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB2<<4)|4: assert(false);break;
        case (0xB2<<4)|5: assert(false);break;
        case (0xB2<<4)|6: assert(false);break;
        case (0xB2<<4)|7: assert(false);break;
        case (0xB2<<4)|8: assert(false);break;
    /* LDA (d,s),y */
        case (0xB3<<4)|0: /* (unimpl) */;break;
        case (0xB3<<4)|1: break;
        case (0xB3<<4)|2: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB3<<4)|3: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB3<<4)|4: assert(false);break;
        case (0xB3<<4)|5: assert(false);break;
        case (0xB3<<4)|6: assert(false);break;
        case (0xB3<<4)|7: assert(false);break;
        case (0xB3<<4)|8: assert(false);break;
    /* LDY d,x */
        case (0xB4<<4)|0: _VPA();_SA(c->PC);break;
        case (0xB4<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0xB4<<4)|2: _SA(c->PC++);break;
        case (0xB4<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0xB4<<4)|4: _YL(c)=_GD();if(_i8(c)){_NZ(_YL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB4<<4)|5: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xB4<<4)|6: assert(false);break;
        case (0xB4<<4)|7: assert(false);break;
        case (0xB4<<4)|8: assert(false);break;
    /* LDA d,x */
        case (0xB5<<4)|0: _VPA();_SA(c->PC);break;
        case (0xB5<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0xB5<<4)|2: _SA(c->PC++);break;
        case (0xB5<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0xB5<<4)|4: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB5<<4)|5: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB5<<4)|6: assert(false);break;
        case (0xB5<<4)|7: assert(false);break;
        case (0xB5<<4)|8: assert(false);break;
    /* LDX d,y */
        case (0xB6<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xB6<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xB6<<4)|2: _SA(c->PC);break;
        case (0xB6<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_Y(c))&0xFF):(c->D+c->AD+_Y(c)));break;
        case (0xB6<<4)|4: _XL(c)=_GD();if(_i8(c)){_NZ(_XL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB6<<4)|5: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xB6<<4)|6: assert(false);break;
        case (0xB6<<4)|7: assert(false);break;
        case (0xB6<<4)|8: assert(false);break;
    /* LDA [d],y */
        case (0xB7<<4)|0: /* (unimpl) */;break;
        case (0xB7<<4)|1: break;
        case (0xB7<<4)|2: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB7<<4)|3: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB7<<4)|4: assert(false);break;
        case (0xB7<<4)|5: assert(false);break;
        case (0xB7<<4)|6: assert(false);break;
        case (0xB7<<4)|7: assert(false);break;
        case (0xB7<<4)|8: assert(false);break;
    /* CLV i */
        case (0xB8<<4)|0: _SA(c->PC);break;
        case (0xB8<<4)|1: c->P&=~0x40;_FETCH();break;
        case (0xB8<<4)|2: assert(false);break;
        case (0xB8<<4)|3: assert(false);break;
        case (0xB8<<4)|4: assert(false);break;
        case (0xB8<<4)|5: assert(false);break;
        case (0xB8<<4)|6: assert(false);break;
        case (0xB8<<4)|7: assert(false);break;
        case (0xB8<<4)|8: assert(false);break;
    /* LDA a,y */
        case (0xB9<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xB9<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xB9<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xB9<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0xB9<<4)|4: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xB9<<4)|5: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xB9<<4)|6: assert(false);break;
        case (0xB9<<4)|7: assert(false);break;
        case (0xB9<<4)|8: assert(false);break;
    /* TSX i */
        case (0xBA<<4)|0: _SA(c->PC);break;
        case (0xBA<<4)|1: if(_i8(c)){_XL(c)=_S(c);_NZ(_XL(c));}else{_X(c)=_S(c);_NZ16(_X(c));}_FETCH();break;
        case (0xBA<<4)|2: assert(false);break;
        case (0xBA<<4)|3: assert(false);break;
        case (0xBA<<4)|4: assert(false);break;
        case (0xBA<<4)|5: assert(false);break;
        case (0xBA<<4)|6: assert(false);break;
        case (0xBA<<4)|7: assert(false);break;
        case (0xBA<<4)|8: assert(false);break;
    /* TYX i */
        case (0xBB<<4)|0: _SA(c->PC);break;
        case (0xBB<<4)|1: if(_i8(c)){_XL(c)=_YL(c);_NZ(_XL(c));}else{_X(c)=_Y(c);_NZ16(_X(c));}_FETCH();break;
        case (0xBB<<4)|2: assert(false);break;
        case (0xBB<<4)|3: assert(false);break;
        case (0xBB<<4)|4: assert(false);break;
        case (0xBB<<4)|5: assert(false);break;
        case (0xBB<<4)|6: assert(false);break;
        case (0xBB<<4)|7: assert(false);break;
        case (0xBB<<4)|8: assert(false);break;
    /* LDY a,x */
        case (0xBC<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xBC<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xBC<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xBC<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0xBC<<4)|4: _YL(c)=_GD();if(_i8(c)){_NZ(_YL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xBC<<4)|5: _YH(c)=_GD();_NZ16(_Y(c));_FETCH();break;
        case (0xBC<<4)|6: assert(false);break;
        case (0xBC<<4)|7: assert(false);break;
        case (0xBC<<4)|8: assert(false);break;
    /* LDA a,x */
        case (0xBD<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xBD<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xBD<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xBD<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0xBD<<4)|4: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xBD<<4)|5: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xBD<<4)|6: assert(false);break;
        case (0xBD<<4)|7: assert(false);break;
        case (0xBD<<4)|8: assert(false);break;
    /* LDX a,y */
        case (0xBE<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xBE<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xBE<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xBE<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0xBE<<4)|4: _XL(c)=_GD();if(_i8(c)){_NZ(_XL(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xBE<<4)|5: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xBE<<4)|6: assert(false);break;
        case (0xBE<<4)|7: assert(false);break;
        case (0xBE<<4)|8: assert(false);break;
    /* LDA al,x */
        case (0xBF<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xBF<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xBF<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0xBF<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));break;
        case (0xBF<<4)|4: _A(c)=_GD();if(_a8(c)){_NZ(_A(c));_FETCH();}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xBF<<4)|5: _B(c)=_GD();_NZ16(_C(c));_FETCH();break;
        case (0xBF<<4)|6: assert(false);break;
        case (0xBF<<4)|7: assert(false);break;
        case (0xBF<<4)|8: assert(false);break;
    /* CPY # */
        case (0xC0<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xC0<<4)|1: if(_a8(c)){_w65816_cmp(c, _YL(c), _GD());_FETCH();}else{c->AD=_GD();_VPA();_SA(c->PC++);}break;
        case (0xC0<<4)|2: _w65816_cmp16(c, _Y(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xC0<<4)|3: assert(false);break;
        case (0xC0<<4)|4: assert(false);break;
        case (0xC0<<4)|5: assert(false);break;
        case (0xC0<<4)|6: assert(false);break;
        case (0xC0<<4)|7: assert(false);break;
        case (0xC0<<4)|8: assert(false);break;
    /* CMP (d,x) */
        case (0xC1<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xC1<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0xC1<<4)|2: _SA(c->PC);break;
        case (0xC1<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0xC1<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0xC1<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xC1<<4)|6: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xC1<<4)|7: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xC1<<4)|8: assert(false);break;
    /* REP # */
        case (0xC2<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xC2<<4)|1: c->P&=~_GD();_SA(c->PC);break;
        case (0xC2<<4)|2: _FETCH();break;
        case (0xC2<<4)|3: assert(false);break;
        case (0xC2<<4)|4: assert(false);break;
        case (0xC2<<4)|5: assert(false);break;
        case (0xC2<<4)|6: assert(false);break;
        case (0xC2<<4)|7: assert(false);break;
        case (0xC2<<4)|8: assert(false);break;
    /* CMP d,s */
        case (0xC3<<4)|0: /* (unimpl) */;break;
        case (0xC3<<4)|1: break;
        case (0xC3<<4)|2: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xC3<<4)|3: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));break;
        case (0xC3<<4)|4: _FETCH();break;
        case (0xC3<<4)|5: assert(false);break;
        case (0xC3<<4)|6: assert(false);break;
        case (0xC3<<4)|7: assert(false);break;
        case (0xC3<<4)|8: assert(false);break;
    /* CPY d */
        case (0xC4<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xC4<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xC4<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xC4<<4)|3: if(_a8(c)){_w65816_cmp(c, _YL(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xC4<<4)|4: _w65816_cmp16(c, _Y(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xC4<<4)|5: assert(false);break;
        case (0xC4<<4)|6: assert(false);break;
        case (0xC4<<4)|7: assert(false);break;
        case (0xC4<<4)|8: assert(false);break;
    /* CMP d */
        case (0xC5<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xC5<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xC5<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xC5<<4)|3: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xC5<<4)|4: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xC5<<4)|5: assert(false);break;
        case (0xC5<<4)|6: assert(false);break;
        case (0xC5<<4)|7: assert(false);break;
        case (0xC5<<4)|8: assert(false);break;
    /* DEC d */
        case (0xC6<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xC6<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xC6<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xC6<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xC6<<4)|4: c->AD|=_GD()<<8;break;
        case (0xC6<<4)|5: _VDA(_GB());c->AD--;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xC6<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xC6<<4)|7: _FETCH();break;
        case (0xC6<<4)|8: assert(false);break;
    /* CMP [d] */
        case (0xC7<<4)|0: /* (unimpl) */;break;
        case (0xC7<<4)|1: break;
        case (0xC7<<4)|2: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xC7<<4)|3: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));break;
        case (0xC7<<4)|4: _FETCH();break;
        case (0xC7<<4)|5: assert(false);break;
        case (0xC7<<4)|6: assert(false);break;
        case (0xC7<<4)|7: assert(false);break;
        case (0xC7<<4)|8: assert(false);break;
    /* INY i */
        case (0xC8<<4)|0: _SA(c->PC);break;
        case (0xC8<<4)|1: if(_i8(c)){_YL(c)++;_NZ(_YL(c));}else{_Y(c)++;_NZ16(_Y(c));}_FETCH();break;
        case (0xC8<<4)|2: assert(false);break;
        case (0xC8<<4)|3: assert(false);break;
        case (0xC8<<4)|4: assert(false);break;
        case (0xC8<<4)|5: assert(false);break;
        case (0xC8<<4)|6: assert(false);break;
        case (0xC8<<4)|7: assert(false);break;
        case (0xC8<<4)|8: assert(false);break;
    /* CMP # */
        case (0xC9<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xC9<<4)|1: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VPA();_SA(c->PC++);}break;
        case (0xC9<<4)|2: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xC9<<4)|3: assert(false);break;
        case (0xC9<<4)|4: assert(false);break;
        case (0xC9<<4)|5: assert(false);break;
        case (0xC9<<4)|6: assert(false);break;
        case (0xC9<<4)|7: assert(false);break;
        case (0xC9<<4)|8: assert(false);break;
    /* DEX i */
        case (0xCA<<4)|0: _SA(c->PC);break;
        case (0xCA<<4)|1: if(_i8(c)){_XL(c)--;_NZ(_XL(c));}else{_X(c)--;_NZ16(_X(c));}_FETCH();break;
        case (0xCA<<4)|2: assert(false);break;
        case (0xCA<<4)|3: assert(false);break;
        case (0xCA<<4)|4: assert(false);break;
        case (0xCA<<4)|5: assert(false);break;
        case (0xCA<<4)|6: assert(false);break;
        case (0xCA<<4)|7: assert(false);break;
        case (0xCA<<4)|8: assert(false);break;
    /* WAI i (unimpl) */
        case (0xCB<<4)|0: _SA(c->PC);break;
        case (0xCB<<4)|1: _FETCH();break;
        case (0xCB<<4)|2: assert(false);break;
        case (0xCB<<4)|3: assert(false);break;
        case (0xCB<<4)|4: assert(false);break;
        case (0xCB<<4)|5: assert(false);break;
        case (0xCB<<4)|6: assert(false);break;
        case (0xCB<<4)|7: assert(false);break;
        case (0xCB<<4)|8: assert(false);break;
    /* CPY a */
        case (0xCC<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xCC<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xCC<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xCC<<4)|3: if(_a8(c)){_w65816_cmp(c, _YL(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xCC<<4)|4: _w65816_cmp16(c, _Y(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xCC<<4)|5: assert(false);break;
        case (0xCC<<4)|6: assert(false);break;
        case (0xCC<<4)|7: assert(false);break;
        case (0xCC<<4)|8: assert(false);break;
    /* CMP a */
        case (0xCD<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xCD<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xCD<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xCD<<4)|3: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xCD<<4)|4: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xCD<<4)|5: assert(false);break;
        case (0xCD<<4)|6: assert(false);break;
        case (0xCD<<4)|7: assert(false);break;
        case (0xCD<<4)|8: assert(false);break;
    /* DEC a */
        case (0xCE<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xCE<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xCE<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xCE<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xCE<<4)|4: c->AD|=_GD()<<8;break;
        case (0xCE<<4)|5: _VDA(_GB());c->AD--;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xCE<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xCE<<4)|7: _FETCH();break;
        case (0xCE<<4)|8: assert(false);break;
    /* CMP al */
        case (0xCF<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xCF<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xCF<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0xCF<<4)|3: _VDA(_GD());_SA(c->AD);break;
        case (0xCF<<4)|4: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xCF<<4)|5: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xCF<<4)|6: assert(false);break;
        case (0xCF<<4)|7: assert(false);break;
        case (0xCF<<4)|8: assert(false);break;
    /* BNE r */
        case (0xD0<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xD0<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x2)!=0x0){_FETCH();};break;
        case (0xD0<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0xD0<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0xD0<<4)|4: assert(false);break;
        case (0xD0<<4)|5: assert(false);break;
        case (0xD0<<4)|6: assert(false);break;
        case (0xD0<<4)|7: assert(false);break;
        case (0xD0<<4)|8: assert(false);break;
    /* CMP (d),y */
        case (0xD1<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xD1<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0xD1<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0xD1<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xD1<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0xD1<<4)|5: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xD1<<4)|6: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xD1<<4)|7: assert(false);break;
        case (0xD1<<4)|8: assert(false);break;
    /* CMP (d) */
        case (0xD2<<4)|0: /* (unimpl) */;break;
        case (0xD2<<4)|1: break;
        case (0xD2<<4)|2: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xD2<<4)|3: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));break;
        case (0xD2<<4)|4: _FETCH();break;
        case (0xD2<<4)|5: assert(false);break;
        case (0xD2<<4)|6: assert(false);break;
        case (0xD2<<4)|7: assert(false);break;
        case (0xD2<<4)|8: assert(false);break;
    /* CMP (d,s),y */
        case (0xD3<<4)|0: /* (unimpl) */;break;
        case (0xD3<<4)|1: break;
        case (0xD3<<4)|2: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xD3<<4)|3: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));break;
        case (0xD3<<4)|4: _FETCH();break;
        case (0xD3<<4)|5: assert(false);break;
        case (0xD3<<4)|6: assert(false);break;
        case (0xD3<<4)|7: assert(false);break;
        case (0xD3<<4)|8: assert(false);break;
    /* PEI s (unimpl) */
        case (0xD4<<4)|0: _SA(c->PC);break;
        case (0xD4<<4)|1: _FETCH();break;
        case (0xD4<<4)|2: assert(false);break;
        case (0xD4<<4)|3: assert(false);break;
        case (0xD4<<4)|4: assert(false);break;
        case (0xD4<<4)|5: assert(false);break;
        case (0xD4<<4)|6: assert(false);break;
        case (0xD4<<4)|7: assert(false);break;
        case (0xD4<<4)|8: assert(false);break;
    /* CMP d,x */
        case (0xD5<<4)|0: _VPA();_SA(c->PC);break;
        case (0xD5<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0xD5<<4)|2: _SA(c->PC++);break;
        case (0xD5<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0xD5<<4)|4: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xD5<<4)|5: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xD5<<4)|6: assert(false);break;
        case (0xD5<<4)|7: assert(false);break;
        case (0xD5<<4)|8: assert(false);break;
    /* DEC d,x */
        case (0xD6<<4)|0: _VPA();_SA(c->PC);break;
        case (0xD6<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0xD6<<4)|2: _SA(c->PC++);break;
        case (0xD6<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0xD6<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xD6<<4)|5: c->AD|=_GD()<<8;break;
        case (0xD6<<4)|6: _VDA(_GB());c->AD--;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xD6<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xD6<<4)|8: _FETCH();break;
    /* CMP [d],y */
        case (0xD7<<4)|0: /* (unimpl) */;break;
        case (0xD7<<4)|1: break;
        case (0xD7<<4)|2: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xD7<<4)|3: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));break;
        case (0xD7<<4)|4: _FETCH();break;
        case (0xD7<<4)|5: assert(false);break;
        case (0xD7<<4)|6: assert(false);break;
        case (0xD7<<4)|7: assert(false);break;
        case (0xD7<<4)|8: assert(false);break;
    /* CLD i */
        case (0xD8<<4)|0: _SA(c->PC);break;
        case (0xD8<<4)|1: c->P&=~0x8;_FETCH();break;
        case (0xD8<<4)|2: assert(false);break;
        case (0xD8<<4)|3: assert(false);break;
        case (0xD8<<4)|4: assert(false);break;
        case (0xD8<<4)|5: assert(false);break;
        case (0xD8<<4)|6: assert(false);break;
        case (0xD8<<4)|7: assert(false);break;
        case (0xD8<<4)|8: assert(false);break;
    /* CMP a,y */
        case (0xD9<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xD9<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xD9<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xD9<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0xD9<<4)|4: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xD9<<4)|5: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xD9<<4)|6: assert(false);break;
        case (0xD9<<4)|7: assert(false);break;
        case (0xD9<<4)|8: assert(false);break;
    /* PHX s */
        case (0xDA<<4)|0: _SA(c->PC);break;
        case (0xDA<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),(_i8(c)?_XL(c):_XH(c)));_WR();break;
        case (0xDA<<4)|2: if(_i8(c)){_FETCH();}else{_VDA(0);_SAD(_SP(_S(c)--),_XL(c));_WR();}break;
        case (0xDA<<4)|3: _FETCH();break;
        case (0xDA<<4)|4: assert(false);break;
        case (0xDA<<4)|5: assert(false);break;
        case (0xDA<<4)|6: assert(false);break;
        case (0xDA<<4)|7: assert(false);break;
        case (0xDA<<4)|8: assert(false);break;
    /* STP i (unimpl) */
        case (0xDB<<4)|0: _SA(c->PC);break;
        case (0xDB<<4)|1: break;
        case (0xDB<<4)|2: _FETCH();break;
        case (0xDB<<4)|3: assert(false);break;
        case (0xDB<<4)|4: assert(false);break;
        case (0xDB<<4)|5: assert(false);break;
        case (0xDB<<4)|6: assert(false);break;
        case (0xDB<<4)|7: assert(false);break;
        case (0xDB<<4)|8: assert(false);break;
    /* JMP [d] */
        case (0xDC<<4)|0: /* (unimpl) */;break;
        case (0xDC<<4)|1: c->PBR=_GB();c->PC=_GA();_FETCH();break;
        case (0xDC<<4)|2: assert(false);break;
        case (0xDC<<4)|3: assert(false);break;
        case (0xDC<<4)|4: assert(false);break;
        case (0xDC<<4)|5: assert(false);break;
        case (0xDC<<4)|6: assert(false);break;
        case (0xDC<<4)|7: assert(false);break;
        case (0xDC<<4)|8: assert(false);break;
    /* CMP a,x */
        case (0xDD<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xDD<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xDD<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xDD<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0xDD<<4)|4: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xDD<<4)|5: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xDD<<4)|6: assert(false);break;
        case (0xDD<<4)|7: assert(false);break;
        case (0xDD<<4)|8: assert(false);break;
    /* DEC a,x */
        case (0xDE<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xDE<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xDE<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0xDE<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0xDE<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xDE<<4)|5: c->AD|=_GD()<<8;break;
        case (0xDE<<4)|6: _VDA(_GB());c->AD--;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xDE<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xDE<<4)|8: _FETCH();break;
    /* CMP al,x */
        case (0xDF<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xDF<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xDF<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0xDF<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));break;
        case (0xDF<<4)|4: if(_a8(c)){_w65816_cmp(c, _A(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xDF<<4)|5: _w65816_cmp16(c, _C(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xDF<<4)|6: assert(false);break;
        case (0xDF<<4)|7: assert(false);break;
        case (0xDF<<4)|8: assert(false);break;
    /* CPX # */
        case (0xE0<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xE0<<4)|1: if(_a8(c)){_w65816_cmp(c, _XL(c), _GD());_FETCH();}else{c->AD=_GD();_VPA();_SA(c->PC++);}break;
        case (0xE0<<4)|2: _w65816_cmp16(c, _X(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xE0<<4)|3: assert(false);break;
        case (0xE0<<4)|4: assert(false);break;
        case (0xE0<<4)|5: assert(false);break;
        case (0xE0<<4)|6: assert(false);break;
        case (0xE0<<4)|7: assert(false);break;
        case (0xE0<<4)|8: assert(false);break;
    /* SBC (d,x) */
        case (0xE1<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xE1<<4)|1: _SA(c->PC);c->AD=_GD();if(!(c->D&0xFF))c->IR++;break;
        case (0xE1<<4)|2: _SA(c->PC);break;
        case (0xE1<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):c->D+c->AD+_X(c));break;
        case (0xE1<<4)|4: _VDA(0);_SA(_E(c)?((c->AD+_X(c)+1)&0xFF):c->D+c->AD+_X(c)+1);c->AD=_GD();break;
        case (0xE1<<4)|5: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xE1<<4)|6: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xE1<<4)|7: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xE1<<4)|8: assert(false);break;
    /* SEP # */
        case (0xE2<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xE2<<4)|1: c->P|=_GD();_SA(c->PC);break;
        case (0xE2<<4)|2: _FETCH();break;
        case (0xE2<<4)|3: assert(false);break;
        case (0xE2<<4)|4: assert(false);break;
        case (0xE2<<4)|5: assert(false);break;
        case (0xE2<<4)|6: assert(false);break;
        case (0xE2<<4)|7: assert(false);break;
        case (0xE2<<4)|8: assert(false);break;
    /* SBC d,s */
        case (0xE3<<4)|0: /* (unimpl) */;break;
        case (0xE3<<4)|1: break;
        case (0xE3<<4)|2: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xE3<<4)|3: _w65816_sbc16(c,c->AD|(_GD()<<8));break;
        case (0xE3<<4)|4: _FETCH();break;
        case (0xE3<<4)|5: assert(false);break;
        case (0xE3<<4)|6: assert(false);break;
        case (0xE3<<4)|7: assert(false);break;
        case (0xE3<<4)|8: assert(false);break;
    /* CPX d */
        case (0xE4<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xE4<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xE4<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xE4<<4)|3: if(_a8(c)){_w65816_cmp(c, _XL(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xE4<<4)|4: _w65816_cmp16(c, _X(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xE4<<4)|5: assert(false);break;
        case (0xE4<<4)|6: assert(false);break;
        case (0xE4<<4)|7: assert(false);break;
        case (0xE4<<4)|8: assert(false);break;
    /* SBC d */
        case (0xE5<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xE5<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xE5<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xE5<<4)|3: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xE5<<4)|4: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xE5<<4)|5: assert(false);break;
        case (0xE5<<4)|6: assert(false);break;
        case (0xE5<<4)|7: assert(false);break;
        case (0xE5<<4)|8: assert(false);break;
    /* INC d */
        case (0xE6<<4)|0: _VPA();_SA(c->PC++);if(_E(c)||(c->D&0xFF)==0)c->IR++;break;
        case (0xE6<<4)|1: c->AD=_GD();_SA(c->PC);break;
        case (0xE6<<4)|2: _VDA(0);if(_E(c)||(c->D&0xFF)==0)c->AD=_GD();_SA((_E(c)?0:c->D)+c->AD);break;
        case (0xE6<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xE6<<4)|4: c->AD|=_GD()<<8;break;
        case (0xE6<<4)|5: _VDA(_GB());c->AD++;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xE6<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xE6<<4)|7: _FETCH();break;
        case (0xE6<<4)|8: assert(false);break;
    /* SBC [d] */
        case (0xE7<<4)|0: /* (unimpl) */;break;
        case (0xE7<<4)|1: break;
        case (0xE7<<4)|2: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xE7<<4)|3: _w65816_sbc16(c,c->AD|(_GD()<<8));break;
        case (0xE7<<4)|4: _FETCH();break;
        case (0xE7<<4)|5: assert(false);break;
        case (0xE7<<4)|6: assert(false);break;
        case (0xE7<<4)|7: assert(false);break;
        case (0xE7<<4)|8: assert(false);break;
    /* INX i */
        case (0xE8<<4)|0: _SA(c->PC);break;
        case (0xE8<<4)|1: if(_i8(c)){_XL(c)++;_NZ(_XL(c));}else{_X(c)++;_NZ16(_X(c));}_FETCH();break;
        case (0xE8<<4)|2: assert(false);break;
        case (0xE8<<4)|3: assert(false);break;
        case (0xE8<<4)|4: assert(false);break;
        case (0xE8<<4)|5: assert(false);break;
        case (0xE8<<4)|6: assert(false);break;
        case (0xE8<<4)|7: assert(false);break;
        case (0xE8<<4)|8: assert(false);break;
    /* SBC # */
        case (0xE9<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xE9<<4)|1: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VPA();_SA(c->PC++);}break;
        case (0xE9<<4)|2: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xE9<<4)|3: assert(false);break;
        case (0xE9<<4)|4: assert(false);break;
        case (0xE9<<4)|5: assert(false);break;
        case (0xE9<<4)|6: assert(false);break;
        case (0xE9<<4)|7: assert(false);break;
        case (0xE9<<4)|8: assert(false);break;
    /* NOP i */
        case (0xEA<<4)|0: _SA(c->PC);break;
        case (0xEA<<4)|1: _FETCH();break;
        case (0xEA<<4)|2: assert(false);break;
        case (0xEA<<4)|3: assert(false);break;
        case (0xEA<<4)|4: assert(false);break;
        case (0xEA<<4)|5: assert(false);break;
        case (0xEA<<4)|6: assert(false);break;
        case (0xEA<<4)|7: assert(false);break;
        case (0xEA<<4)|8: assert(false);break;
    /* XBA i */
        case (0xEB<<4)|0: _SA(c->PC);break;
        case (0xEB<<4)|1: _SA(c->PC);break;
        case (0xEB<<4)|2: _w65816_xba(c);_FETCH();break;
        case (0xEB<<4)|3: assert(false);break;
        case (0xEB<<4)|4: assert(false);break;
        case (0xEB<<4)|5: assert(false);break;
        case (0xEB<<4)|6: assert(false);break;
        case (0xEB<<4)|7: assert(false);break;
        case (0xEB<<4)|8: assert(false);break;
    /* CPX a */
        case (0xEC<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xEC<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xEC<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xEC<<4)|3: if(_a8(c)){_w65816_cmp(c, _XL(c), _GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xEC<<4)|4: _w65816_cmp16(c, _X(c), c->AD|(_GD()<<8));_FETCH();break;
        case (0xEC<<4)|5: assert(false);break;
        case (0xEC<<4)|6: assert(false);break;
        case (0xEC<<4)|7: assert(false);break;
        case (0xEC<<4)|8: assert(false);break;
    /* SBC a */
        case (0xED<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xED<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xED<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xED<<4)|3: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xED<<4)|4: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xED<<4)|5: assert(false);break;
        case (0xED<<4)|6: assert(false);break;
        case (0xED<<4)|7: assert(false);break;
        case (0xED<<4)|8: assert(false);break;
    /* INC a */
        case (0xEE<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xEE<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xEE<<4)|2: _VDA(c->DBR);_SA((_GD()<<8)|c->AD);break;
        case (0xEE<<4)|3: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xEE<<4)|4: c->AD|=_GD()<<8;break;
        case (0xEE<<4)|5: _VDA(_GB());c->AD++;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xEE<<4)|6: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xEE<<4)|7: _FETCH();break;
        case (0xEE<<4)|8: assert(false);break;
    /* SBC al */
        case (0xEF<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xEF<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xEF<<4)|2: _VPA();_SA(c->PC++);c->AD=(_GD()<<8)|c->AD;break;
        case (0xEF<<4)|3: _VDA(_GD());_SA(c->AD);break;
        case (0xEF<<4)|4: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xEF<<4)|5: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xEF<<4)|6: assert(false);break;
        case (0xEF<<4)|7: assert(false);break;
        case (0xEF<<4)|8: assert(false);break;
    /* BEQ r */
        case (0xF0<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xF0<<4)|1: _SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&0x2)!=0x2){_FETCH();};break;
        case (0xF0<<4)|2: _SA((c->PC&0xFF00)|(c->AD&0xFF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};break;
        case (0xF0<<4)|3: c->PC=c->AD;_FETCH();break;
        case (0xF0<<4)|4: assert(false);break;
        case (0xF0<<4)|5: assert(false);break;
        case (0xF0<<4)|6: assert(false);break;
        case (0xF0<<4)|7: assert(false);break;
        case (0xF0<<4)|8: assert(false);break;
    /* SBC (d),y */
        case (0xF1<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xF1<<4)|1: _VDA(c->DBR);c->AD=_GD();_SA(_E(c)?c->AD:(c->D+c->AD));break;
        case (0xF1<<4)|2: _VDA(c->DBR);_SA(_E(c)?((c->AD+1)&0xFF):(c->D+c->AD+1));c->AD=_GD();break;
        case (0xF1<<4)|3: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xF1<<4)|4: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0xF1<<4)|5: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xF1<<4)|6: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xF1<<4)|7: assert(false);break;
        case (0xF1<<4)|8: assert(false);break;
    /* SBC (d) */
        case (0xF2<<4)|0: /* (unimpl) */;break;
        case (0xF2<<4)|1: break;
        case (0xF2<<4)|2: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xF2<<4)|3: _w65816_sbc16(c,c->AD|(_GD()<<8));break;
        case (0xF2<<4)|4: _FETCH();break;
        case (0xF2<<4)|5: assert(false);break;
        case (0xF2<<4)|6: assert(false);break;
        case (0xF2<<4)|7: assert(false);break;
        case (0xF2<<4)|8: assert(false);break;
    /* SBC (d,s),y */
        case (0xF3<<4)|0: /* (unimpl) */;break;
        case (0xF3<<4)|1: break;
        case (0xF3<<4)|2: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xF3<<4)|3: _w65816_sbc16(c,c->AD|(_GD()<<8));break;
        case (0xF3<<4)|4: _FETCH();break;
        case (0xF3<<4)|5: assert(false);break;
        case (0xF3<<4)|6: assert(false);break;
        case (0xF3<<4)|7: assert(false);break;
        case (0xF3<<4)|8: assert(false);break;
    /* PEA s (unimpl) */
        case (0xF4<<4)|0: _SA(c->PC);break;
        case (0xF4<<4)|1: _FETCH();break;
        case (0xF4<<4)|2: assert(false);break;
        case (0xF4<<4)|3: assert(false);break;
        case (0xF4<<4)|4: assert(false);break;
        case (0xF4<<4)|5: assert(false);break;
        case (0xF4<<4)|6: assert(false);break;
        case (0xF4<<4)|7: assert(false);break;
        case (0xF4<<4)|8: assert(false);break;
    /* SBC d,x */
        case (0xF5<<4)|0: _VPA();_SA(c->PC);break;
        case (0xF5<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0xF5<<4)|2: _SA(c->PC++);break;
        case (0xF5<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0xF5<<4)|4: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xF5<<4)|5: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xF5<<4)|6: assert(false);break;
        case (0xF5<<4)|7: assert(false);break;
        case (0xF5<<4)|8: assert(false);break;
    /* INC d,x */
        case (0xF6<<4)|0: _VPA();_SA(c->PC);break;
        case (0xF6<<4)|1: c->AD=_GD();_SA(c->PC);if(_E(c)||(c->D&0xFF)==0){c->IR++;c->PC++;}break;
        case (0xF6<<4)|2: _SA(c->PC++);break;
        case (0xF6<<4)|3: _VDA(0);_SA(_E(c)?((c->AD+_X(c))&0xFF):(c->D+c->AD+_X(c)));break;
        case (0xF6<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xF6<<4)|5: c->AD|=_GD()<<8;break;
        case (0xF6<<4)|6: _VDA(_GB());c->AD++;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xF6<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xF6<<4)|8: _FETCH();break;
    /* SBC [d],y */
        case (0xF7<<4)|0: /* (unimpl) */;break;
        case (0xF7<<4)|1: break;
        case (0xF7<<4)|2: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xF7<<4)|3: _w65816_sbc16(c,c->AD|(_GD()<<8));break;
        case (0xF7<<4)|4: _FETCH();break;
        case (0xF7<<4)|5: assert(false);break;
        case (0xF7<<4)|6: assert(false);break;
        case (0xF7<<4)|7: assert(false);break;
        case (0xF7<<4)|8: assert(false);break;
    /* SED i */
        case (0xF8<<4)|0: _SA(c->PC);break;
        case (0xF8<<4)|1: c->P|=0x8;_FETCH();break;
        case (0xF8<<4)|2: assert(false);break;
        case (0xF8<<4)|3: assert(false);break;
        case (0xF8<<4)|4: assert(false);break;
        case (0xF8<<4)|5: assert(false);break;
        case (0xF8<<4)|6: assert(false);break;
        case (0xF8<<4)|7: assert(false);break;
        case (0xF8<<4)|8: assert(false);break;
    /* SBC a,y */
        case (0xF9<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xF9<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xF9<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_Y(c));c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;break;
        case (0xF9<<4)|3: _VDA(c->DBR);_SA(c->AD+_Y(c));break;
        case (0xF9<<4)|4: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xF9<<4)|5: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xF9<<4)|6: assert(false);break;
        case (0xF9<<4)|7: assert(false);break;
        case (0xF9<<4)|8: assert(false);break;
    /* PLX s */
        case (0xFA<<4)|0: _SA(c->PC);break;
        case (0xFA<<4)|1: _SA(c->PC);break;
        case (0xFA<<4)|2: _VDA(0);_SA(_SP(++_S(c)));break;
        case (0xFA<<4)|3: _XL(c)=_GD();if(_i8(c)){_NZ(_XL(c));_FETCH();}else{_VDA(0);_SA(_SP(++_S(c)));}break;
        case (0xFA<<4)|4: _XH(c)=_GD();_NZ16(_X(c));_FETCH();break;
        case (0xFA<<4)|5: assert(false);break;
        case (0xFA<<4)|6: assert(false);break;
        case (0xFA<<4)|7: assert(false);break;
        case (0xFA<<4)|8: assert(false);break;
    /* XCE i */
        case (0xFB<<4)|0: _SA(c->PC);break;
        case (0xFB<<4)|1: _w65816_xce(c);_FETCH();break;
        case (0xFB<<4)|2: assert(false);break;
        case (0xFB<<4)|3: assert(false);break;
        case (0xFB<<4)|4: assert(false);break;
        case (0xFB<<4)|5: assert(false);break;
        case (0xFB<<4)|6: assert(false);break;
        case (0xFB<<4)|7: assert(false);break;
        case (0xFB<<4)|8: assert(false);break;
    /* JSR (a,x) */
        case (0xFC<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xFC<<4)|1: _VDA(0);_SAD(_SP(_S(c)--),c->PC>>8);_WR();break;
        case (0xFC<<4)|2: _VDA(0);_SAD(_SP(_S(c)--),c->PC);_WR();break;
        case (0xFC<<4)|3: _VPA();_SA(c->PC);break;
        case (0xFC<<4)|4: _SA(c->PC);c->AD=(_GD()<<8)|c->AD;break;
        case (0xFC<<4)|5: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0xFC<<4)|6: _VDA(c->DBR);_SA(c->AD+_X(c)+1);c->AD=_GD();break;
        case (0xFC<<4)|7: c->PC=(_GD()<<8)|c->AD;_FETCH();break;
        case (0xFC<<4)|8: assert(false);break;
    /* SBC a,x */
        case (0xFD<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xFD<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xFD<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;break;
        case (0xFD<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0xFD<<4)|4: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xFD<<4)|5: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xFD<<4)|6: assert(false);break;
        case (0xFD<<4)|7: assert(false);break;
        case (0xFD<<4)|8: assert(false);break;
    /* INC a,x */
        case (0xFE<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xFE<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xFE<<4)|2: c->AD|=_GD()<<8;_SA(c->AD+_X(c));break;
        case (0xFE<<4)|3: _VDA(c->DBR);_SA(c->AD+_X(c));break;
        case (0xFE<<4)|4: c->AD=_GD();if(_a8(c)){c->IR++;if(_E(c)){_WR();}}else{_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xFE<<4)|5: c->AD|=_GD()<<8;break;
        case (0xFE<<4)|6: _VDA(_GB());c->AD++;if(_a8(c)){_NZ(c->AD);_SD(c->AD);}else{_NZ16(c->AD);_SD(c->AD>>8);}_WR();break;
        case (0xFE<<4)|7: if(_a8(c)){_FETCH();}else{_VDA(_GB());_SALD(_GAL()-1,c->AD);_WR();}break;
        case (0xFE<<4)|8: _FETCH();break;
    /* SBC al,x */
        case (0xFF<<4)|0: _VPA();_SA(c->PC++);break;
        case (0xFF<<4)|1: _VPA();_SA(c->PC++);c->AD=_GD();break;
        case (0xFF<<4)|2: _VPA();_SA(c->PC++);c->AD|=_GD()<<8;break;
        case (0xFF<<4)|3: _VDA(_GD());_SA(c->AD+_X(c));break;
        case (0xFF<<4)|4: if(_a8(c)){_w65816_sbc(c,_GD());_FETCH();}else{c->AD=_GD();_VDA(_GB());_SAL(_GAL()+1);}break;
        case (0xFF<<4)|5: _w65816_sbc16(c,c->AD|(_GD()<<8));_FETCH();break;
        case (0xFF<<4)|6: assert(false);break;
        case (0xFF<<4)|7: assert(false);break;
        case (0xFF<<4)|8: assert(false);break;
    // %>
        default: _W65816_UNREACHABLE;
    }
    c->PINS = pins;
    c->irq_pip <<= 1;
    c->nmi_pip <<= 1;
    if (c->emulation) {
        // CPU is in Emulation mode
        // Stack is confined to page 01
        c->S = 0x0100 | (c->S&0xFF);
        // Unused flag is always 1
        c->P |= W65816_UF;
    }
    if (c->emulation | (c->P & W65816_XF)) {
        // CPU is in Emulation mode or registers are in eight-bit mode (X=1)
        // the index registers high byte are zero
        c->X = c->X & 0xFF;
        c->Y = c->Y & 0xFF;
    }
    return pins;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#undef _SA
#undef _GA
#undef _SAD
#undef _SAL
#undef _SB
#undef _GB
#undef _GAL
#undef _SALD
#undef _FETCH
#undef _VPA
#undef _VDA
#undef _SD
#undef _GD
#undef _ON
#undef _OFF
#undef _RD
#undef _WR
#undef _NZ
#undef _NZ16
#undef _Z
#undef _Z16
#endif /* CHIPS_IMPL */
