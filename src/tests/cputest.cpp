#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#define CHIPS_IMPL
#include "chips/w65c816s.h"

#include <ranges>
#include <initializer_list>
#include <tuple>
#include <string>
#include <algorithm>
#include <string>

using namespace std;

typedef tuple<int, string, int, int, bool> instr_data;

// CODE, MNEMONIC, CYCLES, MEM, ENABLE_TEST
initializer_list<instr_data> INSTR_MATRIX = {
    { 0x00, "BRK s",       7, 2, true },
    { 0x01, "ORA (d,x)",   6, 2, true },
    { 0x02, "COP s",       7, 2, true },
    { 0x03, "ORA d,s",     4, 2, true },
    { 0x04, "TSB d",       5, 2, true },
    { 0x05, "ORA d",       3, 2, true },
    { 0x06, "ASL d",       5, 2, true },
    { 0x07, "ORA [d]",     6, 2, true },
    { 0x08, "PHP s",       3, 1, true },
    { 0x09, "ORA #",       2, 2, true },
    { 0x0A, "ASL A",       2, 1, true },
    { 0x0B, "PHD s",       4, 1, true },
    { 0x0C, "TSB a",       6, 3, true },
    { 0x0D, "ORA a",       4, 3, true },
    { 0x0E, "ASL a",       6, 3, true },
    { 0x0F, "ORA al",      5, 4, true },
    { 0x10, "BPL r",       2, 2, true },
    { 0x11, "ORA (d),y",   5, 2, true },
    { 0x12, "ORA (d)",     5, 2, true },
    { 0x13, "ORA (d,s),y", 7, 2, true },
    { 0x14, "TRB d",       5, 2, true },
    { 0x15, "ORA d,x",     4, 2, true },
    { 0x16, "ASL d,x",     6, 2, true },
    { 0x17, "ORA [d],y",   6, 2, true },
    { 0x18, "CLC i",       2, 1, true },
    { 0x19, "ORA a,y",     4, 3, true },
    { 0x1A, "INC A",       2, 1, true },
    { 0x1B, "TCS i",       2, 1, true },
    { 0x1C, "TRB a",       6, 3, true },
    { 0x1D, "ORA a,x",     4, 3, true },
    { 0x1E, "ASL a,x",     7, 3, true },
    { 0x1F, "ORA al,x",    5, 4, true },
    { 0x20, "JSR a",       6, 3, true },
    { 0x21, "AND (d,x)",   6, 2, true },
    { 0x22, "JSL al",      8, 4, true },
    { 0x23, "AND d,s",     4, 2, true },
    { 0x24, "BIT d",       3, 2, true },
    { 0x25, "AND d",       3, 2, true },
    { 0x26, "ROL d",       5, 2, true },
    { 0x27, "AND [d]",     6, 2, true },
    { 0x28, "PLP s",       4, 1, true },
    { 0x29, "AND #",       2, 2, true },
    { 0x2A, "ROL A",       2, 1, true },
    { 0x2B, "PLD s",       5, 1, true },
    { 0x2C, "BIT a",       4, 3, true },
    { 0x2D, "AND a",       4, 3, true },
    { 0x2E, "ROL a",       6, 3, true },
    { 0x2F, "AND al",      5, 4, true },
    { 0x30, "BMI r",       2, 2, true },
    { 0x31, "AND (d),y",   5, 2, true },
    { 0x32, "AND (d)",     5, 2, true },
    { 0x33, "AND (d,s),y", 7, 2, true },
    { 0x34, "BIT d,x",     4, 2, true },
    { 0x35, "AND d,x",     4, 2, true },
    { 0x36, "ROL d,x",     6, 2, true },
    { 0x37, "AND [d],y",   6, 2, true },
    { 0x38, "SEC i",       2, 1, true },
    { 0x39, "AND a,y",     4, 3, true },
    { 0x3A, "DEC A",       2, 1, true },
    { 0x3B, "TSC i",       2, 1, true },
    { 0x3C, "BIT a,x",     4, 3, true },
    { 0x3D, "AND a,x",     4, 3, true },
    { 0x3E, "ROL a,x",     7, 3, true },
    { 0x3F, "AND al,x",    5, 4, true },
    { 0x40, "RTI s",       7, 1, true },
    { 0x41, "EOR (d,x)",   6, 2, true },
    { 0x42, "WDM #",       2, 2, true },
    { 0x43, "EOR d,s",     4, 2, true },
    { 0x44, "MVP xyc",     7, 3, true },
    { 0x45, "EOR d",       3, 2, true },
    { 0x46, "LSR d",       5, 2, true },
    { 0x47, "EOR [d]",     6, 2, true },
    { 0x48, "PHA s",       3, 1, true },
    { 0x49, "EOR #",       2, 2, true },
    { 0x4A, "LSR A",       2, 1, true },
    { 0x4B, "PHK s",       3, 1, true },
    { 0x4C, "JMP a",       3, 3, true },
    { 0x4D, "EOR a",       4, 3, true },
    { 0x4E, "LSR a",       6, 3, true },
    { 0x4F, "EOR al",      5, 4, true },
    { 0x50, "BVC r",       2, 2, true },
    { 0x51, "EOR (d),y",   5, 2, true },
    { 0x52, "EOR (d)",     5, 2, true },
    { 0x53, "EOR (d,s),y", 7, 2, true },
    { 0x54, "MVN xyc",     7, 3, true },
    { 0x55, "EOR d,x",     4, 2, true },
    { 0x56, "LSR d,x",     6, 2, true },
    { 0x57, "EOR [d],y",   6, 2, true },
    { 0x58, "CLI i",       2, 1, true },
    { 0x59, "EOR a,y",     4, 3, true },
    { 0x5A, "PHY s",       3, 1, true },
    { 0x5B, "TCD i",       2, 1, true },
    { 0x5C, "JMP al",      4, 4, true },
    { 0x5D, "EOR a,x",     4, 3, true },
    { 0x5E, "LSR a,x",     7, 3, true },
    { 0x5F, "EOR al,x",    5, 4, true },
    { 0x60, "RTS s",       6, 1, true },
    { 0x61, "ADC (d,x)",   6, 2, true },
    { 0x62, "PER s",       6, 3, true },
    { 0x63, "ADC d,s",     4, 2, true },
    { 0x64, "STZ d",       3, 2, true },
    { 0x65, "ADC d",       3, 2, true },
    { 0x66, "ROR d",       5, 2, true },
    { 0x67, "ADC [d]",     6, 2, true },
    { 0x68, "PLA s",       4, 1, true },
    { 0x69, "ADC #",       2, 2, true },
    { 0x6A, "ROR A",       2, 1, true },
    { 0x6B, "RTL s",       6, 1, true },
    { 0x6C, "JMP (a)",     5, 3, true },
    { 0x6D, "ADC a",       4, 3, true },
    { 0x6E, "ROR a",       6, 3, true },
    { 0x6F, "ADC al",      5, 4, true },
    { 0x70, "BVS r",       2, 2, true },
    { 0x71, "ADC (d),y",   5, 2, true },
    { 0x72, "ADC (d)",     5, 2, true },
    { 0x73, "ADC (d,s),y", 7, 2, true },
    { 0x74, "STZ d,x",     4, 2, true },
    { 0x75, "ADC d,x",     4, 2, true },
    { 0x76, "ROR d,x",     6, 2, true },
    { 0x77, "ADC [d],y",   6, 2, true },
    { 0x78, "SEI i",       2, 1, true },
    { 0x79, "ADC a,y",     4, 3, true },
    { 0x7A, "PLY s",       4, 1, true },
    { 0x7B, "TDC i",       2, 1, true },
    { 0x7C, "JMP (a,x)",   6, 3, true },
    { 0x7D, "ADC a,x",     4, 3, true },
    { 0x7E, "ROR a,x",     7, 3, true },
    { 0x7F, "ADC al,x",    5, 4, true },
    { 0x80, "BRA r",       2, 2, true },
    { 0x81, "STA (d,x)",   6, 2, true },
    { 0x82, "BRL rl",      4, 3, true },
    { 0x83, "STA d,s",     4, 2, true },
    { 0x84, "STY d",       3, 2, true },
    { 0x85, "STA d",       3, 2, true },
    { 0x86, "STX d",       3, 2, true },
    { 0x87, "STA [d]",     6, 2, true },
    { 0x88, "DEY i",       2, 1, true },
    { 0x89, "BIT #",       2, 2, true },
    { 0x8A, "TXA i",       2, 1, true },
    { 0x8B, "PHB s",       3, 1, true },
    { 0x8C, "STY a",       4, 3, true },
    { 0x8D, "STA a",       4, 3, true },
    { 0x8E, "STX a",       4, 3, true },
    { 0x8F, "STA al",      5, 4, true },
    { 0x90, "BCC r",       2, 2, true },
    { 0x91, "STA (d),y",   6, 2, true },
    { 0x92, "STA (d)",     5, 2, true },
    { 0x93, "STA (d,s),y", 7, 2, true },
    { 0x94, "STY d,x",     4, 2, true },
    { 0x95, "STA d,x",     4, 2, true },
    { 0x96, "STX d,y",     4, 2, true },
    { 0x97, "STA [d],y",   6, 2, true },
    { 0x98, "TYA i",       2, 1, true },
    { 0x99, "STA a,y",     5, 3, true },
    { 0x9A, "TXS i",       2, 1, true },
    { 0x9B, "TXY i",       2, 1, true },
    { 0x9C, "STZ a",       4, 3, true },
    { 0x9D, "STA a,x",     5, 3, true },
    { 0x9E, "STZ a,x",     5, 3, true },
    { 0x9F, "STA al,x",    5, 4, true },
    { 0xA0, "LDY #",       2, 2, true },
    { 0xA1, "LDA (d,x)",   6, 2, true },
    { 0xA2, "LDX #",       2, 2, true },
    { 0xA3, "LDA d,s",     4, 2, true },
    { 0xA4, "LDY d",       3, 2, true },
    { 0xA5, "LDA d",       3, 2, true },
    { 0xA6, "LDX d",       3, 2, true },
    { 0xA7, "LDA [d]",     6, 2, true },
    { 0xA8, "TAY i",       2, 1, true },
    { 0xA9, "LDA #",       2, 2, true },
    { 0xAA, "TAX i",       2, 1, true },
    { 0xAB, "PLB s",       4, 1, true },
    { 0xAC, "LDY a",       4, 3, true },
    { 0xAD, "LDA a",       4, 3, true },
    { 0xAE, "LDX a",       4, 3, true },
    { 0xAF, "LDA al",      5, 4, true },
    { 0xB0, "BCS r",       2, 2, true },
    { 0xB1, "LDA (d),y",   5, 2, true },
    { 0xB2, "LDA (d)",     5, 2, true },
    { 0xB3, "LDA (d,s),y", 7, 2, true },
    { 0xB4, "LDY d,x",     4, 2, true },
    { 0xB5, "LDA d,x",     4, 2, true },
    { 0xB6, "LDX d,y",     4, 2, true },
    { 0xB7, "LDA [d],y",   6, 2, true },
    { 0xB8, "CLV i",       2, 1, true },
    { 0xB9, "LDA a,y",     4, 3, true },
    { 0xBA, "TSX i",       2, 1, true },
    { 0xBB, "TYX i",       2, 1, true },
    { 0xBC, "LDY a,x",     4, 3, true },
    { 0xBD, "LDA a,x",     4, 3, true },
    { 0xBE, "LDX a,y",     4, 3, true },
    { 0xBF, "LDA al,x",    5, 4, true },
    { 0xC0, "CPY #",       2, 2, true },
    { 0xC1, "CMP (d,x)",   6, 2, true },
    { 0xC2, "REP #",       3, 2, true },
    { 0xC3, "CMP d,s",     4, 2, true },
    { 0xC4, "CPY d",       3, 2, true },
    { 0xC5, "CMP d",       3, 2, true },
    { 0xC6, "DEC d",       5, 2, true },
    { 0xC7, "CMP [d]",     6, 2, true },
    { 0xC8, "INY i",       2, 1, true },
    { 0xC9, "CMP #",       2, 2, true },
    { 0xCA, "DEX i",       2, 1, true },
    { 0xCB, "WAI i",       3, 1, true },
    { 0xCC, "CPY a",       4, 3, true },
    { 0xCD, "CMP a",       4, 3, true },
    { 0xCE, "DEC a",       6, 3, true },
    { 0xCF, "CMP al",      5, 4, true },
    { 0xD0, "BNE r",       2, 2, true },
    { 0xD1, "CMP (d),y",   5, 2, true },
    { 0xD2, "CMP (d)",     5, 2, true },
    { 0xD3, "CMP (d,s),y", 7, 2, true },
    { 0xD4, "PEI s",       6, 2, true },
    { 0xD5, "CMP d,x",     4, 2, true },
    { 0xD6, "DEC d,x",     6, 2, true },
    { 0xD7, "CMP [d],y",   6, 2, true },
    { 0xD8, "CLD i",       2, 1, true },
    { 0xD9, "CMP a,y",     4, 3, true },
    { 0xDA, "PHX s",       3, 1, true },
    { 0xDB, "STP i",       3, 1, true },
    { 0xDC, "JML (a)",     6, 3, true },
    { 0xDD, "CMP a,x",     4, 3, true },
    { 0xDE, "DEC a,x",     7, 3, true },
    { 0xDF, "CMP al,x",    5, 4, true },
    { 0xE0, "CPX #",       2, 2, true },
    { 0xE1, "SBC (d,x)",   6, 2, true },
    { 0xE2, "SEP #",       3, 2, true },
    { 0xE3, "SBC d,s",     4, 2, true },
    { 0xE4, "CPX d",       3, 2, true },
    { 0xE5, "SBC d",       3, 2, true },
    { 0xE6, "INC d",       5, 2, true },
    { 0xE7, "SBC [d]",     6, 2, true },
    { 0xE8, "INX i",       2, 1, true },
    { 0xE9, "SBC #",       2, 2, true },
    { 0xEA, "NOP i",       2, 1, true },
    { 0xEB, "XBA i",       3, 1, true },
    { 0xEC, "CPX a",       4, 3, true },
    { 0xED, "SBC a",       4, 3, true },
    { 0xEE, "INC a",       6, 3, true },
    { 0xEF, "SBC al",      5, 4, true },
    { 0xF0, "BEQ r",       2, 2, true },
    { 0xF1, "SBC (d),y",   5, 2, true },
    { 0xF2, "SBC (d)",     5, 2, true },
    { 0xF3, "SBC (d,s),y", 7, 2, true },
    { 0xF4, "PEA s",       5, 3, true },
    { 0xF5, "SBC d,x",     4, 2, true },
    { 0xF6, "INC d,x",     6, 2, true },
    { 0xF7, "SBC [d],y",   6, 2, true },
    { 0xF8, "SED i",       2, 1, true },
    { 0xF9, "SBC a,y",     4, 3, true },
    { 0xFA, "PLX s",       4, 1, true },
    { 0xFB, "XCE i",       2, 1, true },
    { 0xFC, "JSR (a,x)",   8, 3, true },
    { 0xFD, "SBC a,x",     4, 3, true },
    { 0xFE, "INC a,x",     7, 3, true },
    { 0xFF, "SBC al,x",    5, 4, true },
};

// Skip failing tests for unimplemented instructions.
const int UNIMPL_skip[] = { 0xCAFE };

#define DOCTEST_VALUE_PARAMETERIZED_DATA(data, data_container)                                                  \
    static size_t _doctest_subcase_idx = 0;                                                                     \
    for_each(data_container.begin(), data_container.end(), [&](const auto& in) {                                \
        DOCTEST_SUBCASE(                                                                                        \
            (string(#data_container "[") + to_string(_doctest_subcase_idx++) + "] => " + get<1>(in)).c_str()) { \
            data = in;                                                                                          \
            REQUIRE(_doctest_subcase_idx - 1 == get<0>(in));                                                    \
        }                                                                                                       \
    });                                                                                                         \
    _doctest_subcase_idx = 0

TEST_CASE("testing instruction matrix") {
    w65816_t cpu;
    w65816_desc_t desc = {};
    uint64_t pins = w65816_init(&cpu, &desc);

    // run the reset sequence - 7 cycles
    for (int _ : ranges::views::iota(0, 7)) {
        W65816_SET_DATA(pins, 0x33);  // provide $3333 reset vector
        pins = w65816_tick(&cpu, pins);
    }

    instr_data data;
    DOCTEST_VALUE_PARAMETERIZED_DATA(data, INSTR_MATRIX);

    auto [instr, mnemonic, instr_cycles, instr_mem, enable_test] = data;
    bool skip = find(begin(UNIMPL_skip), end(UNIMPL_skip), instr) != end(UNIMPL_skip);
    if (enable_test && !skip) {
        CAPTURE(instr);
        CAPTURE(mnemonic);
        CAPTURE(instr_cycles);
        CAPTURE(instr_mem);
        int mem_reads = 0, mem_writes = 0;

        auto pc = w65816_pc(&cpu);
        REQUIRE(pc == 0x3333);
        uint32_t start_pc = pc, end_pc = pc;

        string log;
        CAPTURE(log);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "%02X \"%s\"\n", instr, mnemonic.c_str());
        log += buffer;

        auto cycles = instr_cycles;
        while (cycles--) {
            const bool mem_read = pins & W65816_RW;
            const auto mem_addr = W65816_GET_ADDR(pins);
            // perform memory access
            if (mem_read) {
                // a memory read
                ++mem_reads;
                if (mem_addr == 0x3333) {
                    // provide instruction code
                    W65816_SET_DATA(pins, instr);
                }
                else {
                    // provide marker for instruction data
                    W65816_SET_DATA(pins, 0xAA);
                }
                if (mem_addr == end_pc && (pins & W65816_VPA)) {
                    // advance running program counter if program memory is read
                    end_pc = mem_addr + 1;
                }
            }
            else {
                // a memory write
                ++mem_writes;
            }

            snprintf(
                buffer,
                sizeof(buffer),
                "%s%s%s ADDR: %04X DATA: %02X | PC: %04X \n",
                mem_read ? "R" : "w",
                (pins & W65816_VPA) ? "P" : " ",
                (pins & W65816_VDA) ? "D" : " ",
                mem_addr,
                W65816_GET_DATA(pins),
                pc);
            log += buffer;
            // run the CPU emulation for one tick
            pins = w65816_tick(&cpu, pins);

            pc = w65816_pc(&cpu);
        }
        CAPTURE(mem_reads);
        CAPTURE(mem_writes);

        auto mem_run = end_pc - start_pc;
        CAPTURE(mem_run);

        CHECK(mem_run == instr_mem);

        // CHECK((pins & W65816_VPA));
        // CHECK((pins & W65816_VDA));
    }
}

TEST_CASE("16 bit arithmetic carries") {
    w65816_t cpu;
    w65816_desc_t desc = {};
    uint64_t pins = w65816_init(&cpu, &desc);

    // skip RESET
    pins &= ~(W65816_RW | W65816_RES);
    cpu.PINS = pins;

    uint8_t asm_code[] = {
        0x18,              // clc
        0xfb,              // xce
        0xc2, 0x30,        // rep #$30
        0x18,              // clc
        0xa9, 0x00, 0x80,  // lda #$8000
        0x69, 0x00, 0x90,  // adc #$9000
        0x18,              // clc
        0xa9, 0x00, 0x80,  // lda #$8000
        0x69, 0x00, 0x40,  // adc #$4000
        0x38,              // sec
        0xa9, 0x00, 0x80,  // lda #$8000
        0xe9, 0x00, 0x90,  // sbc #$9000
        0x38,              // sec
        0xa9, 0x00, 0x80,  // lda #$8000
        0xe9, 0x00, 0x40,  // sbc #$4000
    };
    uint8_t flags[] = {
        0x32,  // clc (emulation mode: bits 4 and 5 read as 1)
        0x33,  // xce
        0x03,  // rep #$30
        0x02,  // clc
        0x80,  // lda #$8000
        0x41,  // adc #$9000  ; CS
        0x40,  // clc
        0xC0,  // lda #$8000
        0x80,  // adc #$4000  ; CC
        0x81,  // sec
        0x81,  // lda #$8000
        0x80,  // sbc #$9000  ; CC
        0x81,  // sec
        0x81,  // lda #$8000
        0x41,  // sbc #$4000  ; CS
    };

    size_t code_ptr = 0;
    size_t inst_ptr = 0;

    while (code_ptr < sizeof(asm_code)) {
        if (pins & (W65816_VPA | W65816_VDA)) {
            // memory read
            W65816_SET_DATA(pins, asm_code[code_ptr++]);
        }

        pins = w65816_tick(&cpu, pins);
        // printf("%zu: %016lx %02x\n", code_ptr - 1, pins, cpu.P);

        if ((pins & W65816_VPA) && (pins & W65816_VDA)) {
            // if this is a next instruction fetch, check status register
            CAPTURE(code_ptr);
            CAPTURE(inst_ptr);
            char p_str[9] = {
                (cpu.P & W65816_NF) ? 'N' : '-',   (cpu.P & W65816_VF) ? 'V' : '-', ((cpu.P & W65816_MF) ? 'M' : '-'),
                ((cpu.P & W65816_XF) ? 'X' : '-'), (cpu.P & W65816_DF) ? 'D' : '-', (cpu.P & W65816_IF) ? 'I' : '-',
                (cpu.P & W65816_ZF) ? 'Z' : '-',   (cpu.P & W65816_CF) ? 'C' : '-', '\0'
            };
            CAPTURE(p_str);
            // printf(" >> %s / %zd : %04X\n", p_str, inst_ptr, cpu.C);
            CHECK(cpu.P == flags[inst_ptr++]);
        }
    }
}

TEST_CASE("JSL jumps long") {
    w65816_t cpu;
    w65816_desc_t desc = {};
    uint64_t pins = w65816_init(&cpu, &desc);

    // skip RESET
    pins &= ~(W65816_RW | W65816_RES);
    cpu.PINS = pins;

    // Test code: JSL $123456
    uint8_t memory[] = {
        0x22,
        0x56,
        0x34,
        0x12,
    };

    // Set initial PC to 0x0000, PBR to 0x00
    cpu.PC = 0x0000;
    cpu.PBR = 0x00;
    cpu.S = 0x01FF;  // Stack pointer
    // CPU reads first instruction
    pins |= W65816_RW | W65816_VPA | W65816_VDA;

    std::vector<uint8_t> stack;

    int cycle = 0;

    // Run for 8 cycles (JSL takes 8 cycles according to W65C816S datasheet)
    while (cycle < 8) {
        if (pins & W65816_RW) {
            // memory read
            uint32_t full_addr = W65816_GET_ADDR(pins);
            uint8_t bank = (full_addr >> 16) & 0xFF;
            uint16_t addr = full_addr & 0xFFFF;

            if (bank == 0 && addr < sizeof(memory)) {
                W65816_SET_DATA(pins, memory[addr]);
            }
            else {
                W65816_SET_DATA(pins, 0xEA);  // NOP
            }
        }
        else {
            // writes push to the stack
            stack.push_back((pins >> 16) & 0xFF);
        }

        pins = w65816_tick(&cpu, pins);
        cycle++;
    }

    CAPTURE(stack.size());
    CAPTURE(cpu.PBR);
    CAPTURE(cpu.PC);
    // After JSL $123456, we expect:
    // - PBR = 0x12
    // - PC = 0x3456
    CHECK(cpu.PBR == 0x12);
    CHECK(cpu.PC == 0x3456);
    // and the return address (0x000003) is pushed to the stack
    REQUIRE(stack.size() == 3);
    CHECK(stack[0] == 0x00);  // PBR
    CHECK(stack[1] == 0x00);  // PCH
    CHECK(stack[2] == 0x03);  // PCL
}

// ---------------------------------------------------------------------------
// Behavioural tests against the WDC W65C816S manual
// ---------------------------------------------------------------------------
#include <vector>
#include <set>
#include <sstream>

namespace {

// A 16 MB memory shared by all harness instances (filled with 0xAA, so any
// unprepared operand/pointer/vector reads as $AA/$AAAA/$AAAAAA). Every write
// made through the harness is undone when the harness is destroyed.
struct Harness {
    w65816_t cpu{};
    uint64_t pins = 0;
    static std::vector<uint8_t>& mem() {
        static std::vector<uint8_t> m(1u << 24, 0xAA);
        return m;
    }
    std::vector<std::pair<uint32_t, uint8_t>> dirty;

    // reset into 'start_pc' unless it is left at zero
    explicit Harness(uint16_t start_pc = 0) {
        w65816_desc_t desc = {};
        pins = w65816_init(&cpu, &desc);
        if (start_pc != 0) {
            reset(start_pc);
        }
    }
    ~Harness() {
        for (auto it = dirty.rbegin(); it != dirty.rend(); ++it) {
            mem()[it->first] = it->second;
        }
    }
    void poke(uint32_t addr, uint8_t v) {
        addr &= 0xFFFFFF;
        dirty.push_back({ addr, mem()[addr] });
        mem()[addr] = v;
    }
    void poke(uint32_t addr, std::initializer_list<uint8_t> bytes) {
        for (uint8_t b : bytes) poke(addr++, b);
    }
    uint8_t peek(uint32_t addr) const { return mem()[addr & 0xFFFFFF]; }
    bool fetch() const { return W65816_IS_FETCH(pins); }
    // one bus cycle: serve the memory access requested by 'pins', then tick
    void tick() {
        const uint32_t addr = W65816_GET_ADDR(pins);
        if (pins & W65816_RW) {
            W65816_SET_DATA(pins, mem()[addr]);
        }
        else {
            poke(addr, W65816_GET_DATA(pins));
        }
        pins = w65816_tick(&cpu, pins);
    }
    // run 'n' instructions
    void run(int n) {
        while (n-- > 0) step();
    }
    // run one instruction (pins must be its opcode fetch), return its cycle count
    int step() {
        int n = 0;
        do {
            tick();
            n++;
        } while (!fetch() && n < 64);
        return n;
    }
    // run the reset sequence to 'vec', return the number of cycles it took
    int reset(uint16_t vec) {
        poke(0xFFFC, vec & 0xFF);
        poke(0xFFFD, vec >> 8);
        int n = 0;
        while (!(fetch() && cpu.PC == vec) && n < 32) {
            tick();
            n++;
        }
        return n;
    }
    // switch to native mode with the given M/X bits (register widths)
    void native(bool m16, bool x16) {
        cpu.emulation = 0;
        cpu.P = (cpu.P | W65816_MF | W65816_XF) & ~((m16 ? W65816_MF : 0) | (x16 ? W65816_XF : 0));
    }
};

struct ModeCfg {
    const char* name;
    bool e, m16, x16;
};
const ModeCfg MODES[] = {
    { "emulation", true, false, false },
    { "native M=1 X=1", false, false, false },
    { "native M=0 X=0", false, true, true },
};

const std::set<std::string> USES_M = { "ORA", "AND", "EOR", "ADC", "SBC", "CMP", "LDA", "STA", "BIT", "STZ",
                                       "TSB", "TRB", "ASL", "LSR", "ROL", "ROR", "INC", "DEC", "PHA", "PLA" };
const std::set<std::string> USES_X = { "LDX", "LDY", "STX", "STY", "CPX", "CPY", "PHX", "PHY", "PLX", "PLY" };
const std::set<std::string> RMW = { "ASL", "LSR", "ROL", "ROR", "INC", "DEC", "TSB", "TRB" };
const std::set<std::string> WRITES = { "STA", "STX", "STY", "STZ" };
const std::set<std::string> DIRECT_MODES = { "d", "d,x", "d,y", "(d)", "(d,x)", "(d),y", "[d]", "[d],y" };
const std::set<std::string> INDEXED_PENALTY = { "a,x", "a,y", "(d),y" };

// Expected cycle count derived from the manual's opcode table (emulation
// mode base) plus the Table 5-7 notes: +1 for 16-bit data (+2 for RMW),
// +1 for DL != 0 on direct modes, +1 on indexed reads crossing a page or
// with a 16-bit index, +1 for BRK/COP/RTI in native mode, branches +1 when
// taken and +1 more on a page cross in emulation mode only.
int expected_cycles(const std::string& name, const std::string& mode, int base, const ModeCfg& cfg, bool dl, bool cross, bool taken) {
    const bool native = !cfg.e;
    if (name == "RTI") return native ? 7 : 6;
    // an operand in memory, so the 8/16-bit data width costs a cycle
    const bool memop = (mode != "A" && mode != "i");
    const bool rmw = RMW.count(name) && memop;
    int cyc = base;
    if (name == "BRK" || name == "COP") cyc += native;
    if (USES_M.count(name) && cfg.m16 && memop) cyc += rmw ? 2 : 1;
    if (USES_X.count(name) && cfg.x16 && mode != "i") cyc += 1;
    if ((DIRECT_MODES.count(mode) || name == "PEI") && dl) cyc += 1;
    if (INDEXED_PENALTY.count(mode) && !WRITES.count(name) && !rmw && (cross || cfg.x16)) cyc += 1;
    if (mode == "r" && taken) {
        cyc += 1;
        if (cross && cfg.e) cyc += 1;
    }
    return cyc;
}

// P value that makes the branch 'name' taken or not
uint8_t branch_flags(const std::string& name, bool taken) {
    if (name == "BRA") return 0;  // always taken, tests no flag
    const uint8_t f = (name == "BPL" || name == "BMI")   ? W65816_NF
                      : (name == "BVC" || name == "BVS") ? W65816_VF
                      : (name == "BCC" || name == "BCS") ? W65816_CF
                                                        : W65816_ZF;
    // BPL/BVC/BCC/BNE branch when their flag is clear
    const bool inverted = (name == "BPL" || name == "BVC" || name == "BCC" || name == "BNE");
    return (taken != inverted) ? f : 0;
}

}  // namespace

TEST_CASE("instruction cycle counts per mode") {
    for (const ModeCfg& cfg : MODES) {
        for (bool dl : { false, true }) {
            for (bool cross : { false, true }) {
                for (const auto& [code, mnemonic, base, mem_bytes, enable] : INSTR_MATRIX) {
                    if (!enable || find(begin(UNIMPL_skip), end(UNIMPL_skip), code) != end(UNIMPL_skip)) continue;
                    std::istringstream is(mnemonic);
                    std::string name, mode;
                    is >> name >> mode;
                    // both park the CPU instead of fetching the next opcode
                    if (name == "WAI" || name == "STP") continue;
                    for (bool taken : { false, true }) {
                        if (mode != "r" && taken) continue;
                        if (name == "BRA" && !taken) continue;  // BRA is unconditional
                        CAPTURE(cfg.name);
                        CAPTURE(dl);
                        CAPTURE(cross);
                        CAPTURE(taken);
                        CAPTURE(mnemonic);
                        // a branch with offset $AA (-86) from $4002 crosses the page, from $4082 it does not
                        const uint16_t start = (mode == "r" && !cross) ? 0x4080 : 0x4000;
                        Harness h(start);
                        if (!cfg.e) h.native(cfg.m16, cfg.x16);
                        h.cpu.D = dl ? 0x0034 : 0x0000;
                        h.cpu.X = h.cpu.Y = cross ? 0x0080 : 0x0000;  // $AAAA + $80 crosses into $AB
                        h.cpu.C = 0;  // one MVN/MVP iteration
                        h.cpu.S = 0x01FF;
                        if (mode == "r") h.cpu.P = (h.cpu.P & 0x30) | branch_flags(name, taken);
                        h.poke(start, (uint8_t)code);
                        const int cycles = h.step();
                        CHECK(cycles == expected_cycles(name, mode, base, cfg, dl, cross, taken));
                    }
                }
            }
        }
    }
}

TEST_CASE("WAI and STP park on their third cycle") {
    for (uint8_t op : { (uint8_t)0xCB, (uint8_t)0xDB }) {
        Harness h(0x4000);
        h.poke(0x4000, op);
        int n = 0;
        while (!h.cpu.stopped && n < 10) {
            h.tick();
            n++;
        }
        // the opcode fetch and one internal cycle run before parking, so the
        // parked cycle is the instruction's third
        CHECK(n == 2);
    }
}

TEST_CASE("emulation mode direct page uses D and wraps only when DL == 0") {
    SUBCASE("LDA d with DH != 0, DL == 0") {
        Harness h(0x4000);
        h.cpu.D = 0x1200;
        h.poke(0x4000, { 0xA5, 0x10 });  // LDA $10
        h.poke(0x1210, 0x42);
        CHECK(h.step() == 3);
        CHECK(h.cpu.C == 0x0042);
    }
    SUBCASE("LDA d with DL != 0 costs a cycle and does not wrap") {
        Harness h(0x4000);
        h.cpu.D = 0x1234;
        h.poke(0x4000, { 0xA5, 0x10 });
        h.poke(0x1244, 0x43);
        CHECK(h.step() == 4);
        CHECK(h.cpu.C == 0x0043);
    }
    SUBCASE("LDA d,x wraps within the direct page when DL == 0") {
        Harness h(0x4000);
        h.cpu.D = 0x1200;
        h.cpu.X = 0x20;
        h.poke(0x4000, { 0xB5, 0xF0 });  // LDA $F0,X
        h.poke(0x1210, 0x44);
        h.poke(0x1310, 0x55);
        h.step();
        CHECK(h.cpu.C == 0x0044);
    }
    SUBCASE("LDA d,x does not wrap when DL != 0") {
        Harness h(0x4000);
        h.cpu.D = 0x1234;
        h.cpu.X = 0x20;
        h.poke(0x4000, { 0xB5, 0xF0 });
        h.poke(0x1344, 0x45);
        h.step();
        CHECK(h.cpu.C == 0x0045);
    }
    SUBCASE("(d) pointer high byte wraps within the page, [d] and PEI do not") {
        Harness h(0x4000);
        h.cpu.D = 0x1200;
        h.poke(0x12FF, 0x00);  // pointer low
        h.poke(0x1200, 0x20);  // wrapped pointer high -> $2000
        h.poke(0x1300, 0x30);  // unwrapped pointer high -> $3000
        h.poke(0x1301, 0x01);  // bank for [d] -> $013000
        h.poke(0x2000, 0x11);
        h.poke(0x3000, 0x22);
        h.poke(0x013000, 0x33);
        h.poke(0x4000, { 0xB2, 0xFF });  // LDA ($FF)
        h.step();
        CHECK(h.cpu.C == 0x0011);
        h.poke(0x4002, { 0xA7, 0xFF });  // LDA [$FF]
        h.step();
        CHECK(h.cpu.C == 0x0033);
        h.poke(0x4004, { 0xD4, 0xFF });  // PEI ($FF)
        h.cpu.S = 0x01FF;
        h.step();
        CHECK(h.peek(0x01FF) == 0x30);
        CHECK(h.peek(0x01FE) == 0x00);
    }
}

TEST_CASE("(d),y pointer is in bank 0 and the effective address carries into the bank") {
    Harness h(0x4000);
    h.native(false, false);
    h.cpu.DBR = 0x02;
    h.cpu.Y = 0x20;
    h.poke(0x0010, { 0xF0, 0xFF });  // pointer $FFF0 in bank 0
    h.poke(0x020010, 0x66);          // wrong: no carry
    h.poke(0x030010, 0x77);          // right: $02FFF0 + $20 = $030010
    h.poke(0x4000, { 0xB1, 0x10 });  // LDA ($10),Y
    h.step();
    CHECK(h.cpu.C == 0x0077);
}

TEST_CASE("absolute and long indexed addressing carries into the next bank") {
    Harness h(0x4000);
    h.native(false, false);
    h.cpu.DBR = 0x01;
    h.cpu.X = 0x20;
    h.poke(0x020010, 0x88);
    h.poke(0x4000, { 0xBD, 0xF0, 0xFF });  // LDA $FFF0,X -> $020010
    h.step();
    CHECK(h.cpu.C == 0x0088);
    h.poke(0x030010, 0x99);
    h.poke(0x4003, { 0xBF, 0xF0, 0xFF, 0x02 });  // LDA $02FFF0,X -> $030010
    h.step();
    CHECK(h.cpu.C == 0x0099);
}

TEST_CASE("indexed reads without a page cross use the data bank") {
    // The cycle that would carry the index is skipped for an 8-bit index that
    // stays inside the page; the data read then happens in that cycle and must
    // still be a valid data access in the data bank, not the program bank.
    Harness h(0x4000);
    h.native(false, false);
    h.cpu.DBR = 0x7E;
    h.cpu.X = 0x02;
    h.cpu.Y = 0x02;
    h.poke(0x007002, 0x11);  // program bank copy
    h.poke(0x7E7002, 0x22);  // data bank copy
    h.poke(0x4000, { 0xBD, 0x00, 0x70 });  // LDA $7000,X
    CHECK(h.step() == 4);                  // no page cross, no penalty cycle
    CHECK(h.cpu.C == 0x0022);
    h.poke(0x4003, { 0xB9, 0x00, 0x70 });  // LDA $7000,Y
    h.step();
    CHECK(h.cpu.C == 0x0022);
    h.poke(0x0020, { 0x00, 0x70 });        // pointer in bank 0
    h.poke(0x4006, { 0xB1, 0x20 });        // LDA ($20),Y
    h.step();
    CHECK(h.cpu.C == 0x0022);
}

TEST_CASE("indirect jumps use bank 0 for (a)/[a] and the program bank for (a,x)") {
    Harness h(0x4000);
    h.native(false, false);
    // move the program to bank 1
    h.poke(0x4000, { 0x5C, 0x00, 0x50, 0x01 });  // JML $015000
    h.step();
    REQUIRE(h.cpu.PBR == 0x01);
    REQUIRE(h.cpu.PC == 0x5000);
    h.cpu.DBR = 0x02;
    h.cpu.X = 0x02;
    h.poke(0x002000, { 0x00, 0x60 });        // bank 0 table: JMP (a) -> $6000
    h.poke(0x012000, { 0x00, 0x70 });        // bank 1 table
    h.poke(0x022000, { 0x00, 0x80 });        // bank 2 table
    h.poke(0x012002, { 0x00, 0x71 });        // bank 1 table + X: JMP (a,x) -> $7100
    h.poke(0x022002, { 0x00, 0x81 });
    h.poke(0x002002, { 0x00, 0x61 });
    h.poke(0x015000, { 0x6C, 0x00, 0x20 });  // JMP ($2000)
    h.step();
    CHECK(h.cpu.PBR == 0x01);
    CHECK(h.cpu.PC == 0x6000);
    h.poke(0x016000, { 0x7C, 0x00, 0x20 });  // JMP ($2000,X)
    h.step();
    CHECK(h.cpu.PBR == 0x01);
    CHECK(h.cpu.PC == 0x7100);
    h.cpu.S = 0x01FF;
    h.poke(0x017100, { 0xFC, 0x00, 0x20 });  // JSR ($2000,X)
    h.step();
    CHECK(h.cpu.PBR == 0x01);
    CHECK(h.cpu.PC == 0x7100);
    h.poke(0x002100, { 0x00, 0x90, 0x03 });  // bank 0 table: JML [a] -> $039000
    h.poke(0x012100, { 0x00, 0x91, 0x04 });
    h.poke(0x017100, { 0xDC, 0x00, 0x21 });  // JML [$2100]
    h.step();
    CHECK(h.cpu.PBR == 0x03);
    CHECK(h.cpu.PC == 0x9000);
}

TEST_CASE("decimal mode arithmetic") {
    SUBCASE("8-bit ADC: N and Z reflect the adjusted result") {
        Harness h(0x4000);
        h.cpu.P |= W65816_DF;
        h.poke(0x4000, { 0x18, 0xA9, 0x99, 0x69, 0x01 });  // CLC, LDA #$99, ADC #$01
        h.run(3);
        CHECK(h.cpu.C == 0x0000);
        CHECK((h.cpu.P & W65816_CF));
        CHECK((h.cpu.P & W65816_ZF));
        CHECK(!(h.cpu.P & W65816_NF));
    }
    SUBCASE("8-bit SBC: N from the adjusted result") {
        Harness h(0x4000);
        h.cpu.P |= W65816_DF;
        h.poke(0x4000, { 0x38, 0xA9, 0x00, 0xE9, 0x80 });  // SEC, LDA #$00, SBC #$80
        h.run(3);
        CHECK(h.cpu.C == 0x0020);
        CHECK(!(h.cpu.P & W65816_NF));
        CHECK(!(h.cpu.P & W65816_CF));
    }
    SUBCASE("16-bit ADC") {
        Harness h(0x4000);
        h.native(true, false);
        h.cpu.P |= W65816_DF;
        h.poke(0x4000, { 0x18, 0xA9, 0x34, 0x12, 0x69, 0x78, 0x56 });  // CLC, LDA #$1234, ADC #$5678
        h.run(3);
        CHECK(h.cpu.C == 0x6912);
        CHECK(!(h.cpu.P & W65816_CF));
        CHECK(!(h.cpu.P & W65816_ZF));
        h.poke(0x4007, { 0x18, 0xA9, 0x99, 0x99, 0x69, 0x01, 0x00 });  // CLC, LDA #$9999, ADC #$0001
        h.run(3);
        CHECK(h.cpu.C == 0x0000);
        CHECK((h.cpu.P & W65816_CF));
        CHECK((h.cpu.P & W65816_ZF));
        CHECK(!(h.cpu.P & W65816_NF));
    }
    SUBCASE("16-bit SBC") {
        Harness h(0x4000);
        h.native(true, false);
        h.cpu.P |= W65816_DF;
        h.poke(0x4000, { 0x38, 0xA9, 0x00, 0x50, 0xE9, 0x34, 0x12 });  // SEC, LDA #$5000, SBC #$1234
        h.run(3);
        CHECK(h.cpu.C == 0x3766);
        CHECK((h.cpu.P & W65816_CF));
        h.poke(0x4007, { 0x38, 0xA9, 0x00, 0x00, 0xE9, 0x01, 0x00 });  // SEC, LDA #$0000, SBC #$0001
        h.run(3);
        CHECK(h.cpu.C == 0x9999);
        CHECK(!(h.cpu.P & W65816_CF));
        CHECK((h.cpu.P & W65816_NF));
    }
}

TEST_CASE("BIT flags") {
    SUBCASE("BIT # affects only Z") {
        Harness h(0x4000);
        h.cpu.P = (h.cpu.P | W65816_VF) & ~W65816_NF;
        h.poke(0x4000, { 0xA9, 0x0F, 0x89, 0xF0 });  // LDA #$0F, BIT #$F0
        h.step();
        h.step();
        CHECK((h.cpu.P & W65816_ZF));
        CHECK((h.cpu.P & W65816_VF));
        CHECK(!(h.cpu.P & W65816_NF));
    }
    SUBCASE("16-bit BIT takes N and V from bits 15 and 14") {
        Harness h(0x4000);
        h.native(true, false);
        h.poke(0x2000, { 0x00, 0x40 });                // $4000: bit 14 set
        h.poke(0x4000, { 0xA9, 0xFF, 0xFF, 0x2C, 0x00, 0x20 });  // LDA #$FFFF, BIT $2000
        h.step();
        h.step();
        CHECK(!(h.cpu.P & W65816_NF));
        CHECK((h.cpu.P & W65816_VF));
        CHECK(!(h.cpu.P & W65816_ZF));
    }
}

TEST_CASE("XBA, TSC and TDC set 16-bit N and Z") {
    Harness h(0x4000);
    h.native(false, false);
    h.cpu.C = 0x8000;                // A=$00, B=$80
    h.poke(0x4000, 0xEB);            // XBA
    CHECK(h.step() == 3);
    CHECK(h.cpu.C == 0x0080);
    CHECK((h.cpu.P & W65816_NF));
    CHECK(!(h.cpu.P & W65816_ZF));
    h.cpu.S = 0x0100;
    h.poke(0x4001, 0x3B);            // TSC
    CHECK(h.step() == 2);
    CHECK(h.cpu.C == 0x0100);
    CHECK(!(h.cpu.P & W65816_ZF));
    CHECK(!(h.cpu.P & W65816_NF));
    h.cpu.D = 0x8000;
    h.poke(0x4002, 0x7B);            // TDC
    h.step();
    CHECK(h.cpu.C == 0x8000);
    CHECK((h.cpu.P & W65816_NF));
}

TEST_CASE("XCE leaves M and X alone when staying in native mode") {
    Harness h(0x4000);
    h.poke(0x4000, { 0x18, 0xFB, 0xC2, 0x30, 0x18, 0xFB, 0x38, 0xFB });  // CLC XCE REP#$30 CLC XCE SEC XCE
    h.step();
    h.step();
    CHECK(!h.cpu.emulation);
    CHECK((h.cpu.P & (W65816_MF | W65816_XF)) == (W65816_MF | W65816_XF));
    h.step();
    h.step();
    h.step();  // CLC; XCE while native: no change
    CHECK(!h.cpu.emulation);
    CHECK((h.cpu.P & (W65816_MF | W65816_XF)) == 0);
    h.step();
    h.step();  // SEC; XCE -> emulation, M/X forced
    CHECK(h.cpu.emulation);
    CHECK((h.cpu.P & (W65816_MF | W65816_XF)) == (W65816_MF | W65816_XF));
    CHECK((h.cpu.S & 0xFF00) == 0x0100);
}

TEST_CASE("emulation mode P bits 4 and 5, and the pushed B flag") {
    Harness h(0x4000);
    h.cpu.S = 0x01FF;
    h.poke(0x4000, { 0xC2, 0x30, 0x08 });  // REP #$30, PHP
    h.step();
    CHECK((h.cpu.P & 0x30) == 0x30);
    h.step();
    CHECK((h.peek(0x01FF) & 0x30) == 0x30);
    // BRK pushes B=1 and clears PBR, IRQ pushes B=0
    h.poke(0xFFFE, { 0x00, 0x50 });
    h.poke(0x4003, { 0x00, 0x00 });  // BRK
    h.cpu.PBR = 0x01;                // program bank is ignored for the vector
    h.poke(0x014003, { 0x00, 0x00 });
    CHECK(h.step() == 7);
    CHECK(h.cpu.PBR == 0x00);
    CHECK(h.cpu.PC == 0x5000);
    CHECK((h.peek(0x01FC) & 0x30) == 0x30);
    CHECK((h.cpu.P & W65816_IF));
    h.poke(0x5000, 0xEA);  // NOP, interrupted by IRQ
    h.cpu.P &= ~W65816_IF;
    h.pins |= W65816_IRQ;
    h.step();              // NOP
    h.pins |= W65816_IRQ;
    h.step();              // IRQ sequence (7 cycles), vector $FFFE -> $5000
    CHECK(h.cpu.PC == 0x5000);
    CHECK((h.peek(0x01F9) & 0x30) == 0x20);
}

TEST_CASE("reset conditions the registers and always runs the 7-cycle sequence") {
    Harness h(0x4000);
    h.native(true, true);
    h.cpu.D = 0x1234;
    h.cpu.DBR = 0x05;
    h.cpu.PBR = 0x06;
    h.cpu.S = 0x7FFF;
    h.cpu.X = 0x1234;
    h.cpu.P |= W65816_DF;
    h.poke(0x064000, 0xEA);
    h.step();
    h.pins |= W65816_RES;
    CHECK(h.reset(0x4000) == 7);
    CHECK(h.cpu.emulation);
    CHECK(h.cpu.D == 0);
    CHECK(h.cpu.DBR == 0);
    CHECK(h.cpu.PBR == 0);
    CHECK((h.cpu.S & 0xFF00) == 0x0100);
    CHECK((h.cpu.X & 0xFF00) == 0);
    CHECK((h.cpu.P & (W65816_MF | W65816_XF | W65816_IF)) == (W65816_MF | W65816_XF | W65816_IF));
    CHECK(!(h.cpu.P & W65816_DF));
}

TEST_CASE("RDY stalls write cycles") {
    Harness h(0x4000);
    h.poke(0x4000, { 0xA9, 0x5A, 0x8D, 0x00, 0x20 });  // LDA #$5A, STA $2000
    h.step();
    // run STA until the write cycle is on the bus
    while (h.pins & W65816_RW) h.tick();
    REQUIRE(W65816_GET_ADDR(h.pins) == 0x2000);
    // hold RDY for a few cycles: the write must be repeated, not consumed
    for (int i = 0; i < 3; i++) {
        h.pins |= W65816_RDY;
        h.pins = w65816_tick(&h.cpu, h.pins);
        CHECK(!(h.pins & W65816_RW));
        CHECK(W65816_GET_ADDR(h.pins) == 0x2000);
    }
    h.pins &= ~W65816_RDY;
    h.tick();
    CHECK(h.peek(0x2000) == 0x5A);
}

TEST_CASE("VPB is asserted during the two vector fetch cycles") {
    Harness h(0x4000);
    h.poke(0x4000, { 0x00, 0x00 });
    std::vector<uint32_t> vpb_addrs;
    int n = 0;
    do {
        h.tick();
        if (h.pins & W65816_VPB) vpb_addrs.push_back(W65816_GET_ADDR(h.pins));
        n++;
    } while (!h.fetch() && n < 16);
    REQUIRE(vpb_addrs.size() == 2);
    CHECK(vpb_addrs[0] == 0xFFFE);
    CHECK(vpb_addrs[1] == 0xFFFF);
}
