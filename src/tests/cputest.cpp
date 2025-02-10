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
    { 0x00, "BRK s",       7, 2, true  },
    { 0x01, "ORA (d,x)",   6, 2, true  },
    { 0x02, "COP s",       7, 2, false },
    { 0x03, "ORA d,s",     4, 2, false },
    { 0x04, "TSB d",       5, 2, true  },
    { 0x05, "ORA d",       3, 2, true  },
    { 0x06, "ASL d",       5, 2, true  },
    { 0x07, "ORA [d]",     6, 2, false },
    { 0x08, "PHP s",       3, 1, true  },
    { 0x09, "ORA #",       2, 2, true  },
    { 0x0A, "ASL A",       2, 1, true  },
    { 0x0B, "PHD s",       4, 1, false },
    { 0x0C, "TSB a",       6, 3, true  },
    { 0x0D, "ORA a",       4, 3, true  },
    { 0x0E, "ASL a",       6, 3, true  },
    { 0x0F, "ORA al",      5, 4, false },
    { 0x10, "BPL r",       2, 2, true  },
    { 0x11, "ORA (d),y",   5, 2, true  },
    { 0x12, "ORA (d)",     5, 2, true  },
    { 0x13, "ORA (d,s),y", 7, 2, false },
    { 0x14, "TRB d",       5, 2, true  },
    { 0x15, "ORA d,x",     4, 2, true  },
    { 0x16, "ASL d,x",     6, 2, true  },
    { 0x17, "ORA [d],y",   6, 2, false },
    { 0x18, "CLC i",       2, 1, true  },
    { 0x19, "ORA a,y",     4, 3, true  },
    { 0x1A, "INC A",       2, 1, true  },
    { 0x1B, "TCS i",       2, 1, false },
    { 0x1C, "TRB a",       6, 3, true  },
    { 0x1D, "ORA a,x",     4, 3, true  },
    { 0x1E, "ASL a,x",     7, 3, true  },
    { 0x1F, "ORA al,x",    5, 4, false },
    { 0x20, "JSR a",       6, 3, true  },
    { 0x21, "AND (d,x)",   6, 2, true  },
    { 0x22, "JSL al",      8, 4, false },
    { 0x23, "AND d,s",     4, 2, false },
    { 0x24, "BIT d",       3, 2, true  },
    { 0x25, "AND d",       3, 2, true  },
    { 0x26, "ROL d",       5, 2, true  },
    { 0x27, "AND [d]",     6, 2, false },
    { 0x28, "PLP s",       4, 1, true  },
    { 0x29, "AND #",       2, 2, true  },
    { 0x2A, "ROL A",       2, 1, true  },
    { 0x2B, "PLD s",       5, 1, false },
    { 0x2C, "BIT a",       4, 3, true  },
    { 0x2D, "AND a",       4, 3, true  },
    { 0x2E, "ROL a",       6, 3, true  },
    { 0x2F, "AND al",      5, 4, false },
    { 0x30, "BMI r",       2, 2, true  },
    { 0x31, "AND (d),y",   5, 2, true  },
    { 0x32, "AND (d)",     5, 2, true  },
    { 0x33, "AND (d,s),y", 7, 2, false },
    { 0x34, "BIT d,x",     4, 2, true  },
    { 0x35, "AND d,x",     4, 2, true  },
    { 0x36, "ROL d,x",     6, 2, true  },
    { 0x37, "AND [d],y",   6, 2, false },
    { 0x38, "SEC i",       2, 1, true  },
    { 0x39, "AND a,y",     4, 3, true  },
    { 0x3A, "DEC A",       2, 1, true  },
    { 0x3B, "TSC i",       2, 1, false },
    { 0x3C, "BIT a,x",     4, 3, true  },
    { 0x3D, "AND a,x",     4, 3, true  },
    { 0x3E, "ROL a,x",     7, 3, true  },
    { 0x3F, "AND al,x",    5, 4, false },
    { 0x40, "RTI s",       7, 1, false },
    { 0x41, "EOR (d,x)",   6, 2, true  },
    { 0x42, "WDM i",       2, 2, false },
    { 0x43, "EOR d,s",     4, 2, false },
    { 0x44, "MVP xyc",     7, 3, false },
    { 0x45, "EOR d",       3, 2, true  },
    { 0x46, "LSR d",       5, 2, true  },
    { 0x47, "EOR [d]",     6, 2, false },
    { 0x48, "PHA s",       3, 1, true  },
    { 0x49, "EOR #",       2, 2, true  },
    { 0x4A, "LSR A",       2, 1, true  },
    { 0x4B, "PHK s",       3, 1, false },
    { 0x4C, "JMP a",       3, 3, true  },
    { 0x4D, "EOR a",       4, 3, true  },
    { 0x4E, "LSR a",       6, 3, true  },
    { 0x4F, "EOR al",      5, 4, false },
    { 0x50, "BVC r",       2, 2, true  },
    { 0x51, "EOR (d),y",   5, 2, true  },
    { 0x52, "EOR (d)",     5, 2, true  },
    { 0x53, "EOR (d,s),y", 7, 2, false },
    { 0x54, "MVN xyc",     7, 3, false },
    { 0x55, "EOR d,x",     4, 2, true  },
    { 0x56, "LSR d,x",     6, 2, true  },
    { 0x57, "EOR [d],y",   6, 2, false },
    { 0x58, "CLI i",       2, 1, true  },
    { 0x59, "EOR a,y",     4, 3, true  },
    { 0x5A, "PHY s",       3, 1, true  },
    { 0x5B, "TCD i",       2, 1, false },
    { 0x5C, "JMP al",      4, 4, false },
    { 0x5D, "EOR a,x",     4, 3, true  },
    { 0x5E, "LSR a,x",     7, 3, true  },
    { 0x5F, "EOR al,x",    5, 4, false },
    { 0x60, "RTS s",       6, 1, false },
    { 0x61, "ADC (d,x)",   6, 2, true  },
    { 0x62, "PER s",       6, 3, false },
    { 0x63, "ADC d,s",     4, 2, false },
    { 0x64, "STZ d",       3, 2, true  },
    { 0x65, "ADC d",       3, 2, true  },
    { 0x66, "ROR d",       5, 2, true  },
    { 0x67, "ADC [d]",     6, 2, false },
    { 0x68, "PLA s",       4, 1, true  },
    { 0x69, "ADC #",       2, 2, true  },
    { 0x6A, "ROR A",       2, 1, true  },
    { 0x6B, "RTL s",       6, 1, false },
    { 0x6C, "JMP (a)",     5, 3, true  },
    { 0x6D, "ADC a",       4, 3, true  },
    { 0x6E, "ROR a",       6, 3, true  },
    { 0x6F, "ADC al",      5, 4, false },
    { 0x70, "BVS r",       2, 2, true  },
    { 0x71, "ADC (d),y",   5, 2, true  },
    { 0x72, "ADC (d)",     5, 2, true  },
    { 0x73, "ADC (d,s),y", 7, 2, false },
    { 0x74, "STZ d,x",     4, 2, true  },
    { 0x75, "ADC d,x",     4, 2, true  },
    { 0x76, "ROR d,x",     6, 2, true  },
    { 0x77, "ADC [d],y",   6, 2, false },
    { 0x78, "SEI i",       2, 1, true  },
    { 0x79, "ADC a,y",     4, 3, true  },
    { 0x7A, "PLY s",       4, 1, true  },
    { 0x7B, "TDC i",       2, 1, false },
    { 0x7C, "JMP (a,x)",   6, 3, true  },
    { 0x7D, "ADC a,x",     4, 3, true  },
    { 0x7E, "ROR a,x",     7, 3, true  },
    { 0x7F, "ADC al,x",    5, 4, false },
    { 0x80, "BRA r",       2, 2, true  },
    { 0x81, "STA (d,x)",   6, 2, true  },
    { 0x82, "BRL rl",      4, 3, false },
    { 0x83, "STA d,s",     4, 2, false },
    { 0x84, "STY d",       3, 2, true  },
    { 0x85, "STA d",       3, 2, true  },
    { 0x86, "STX d",       3, 2, true  },
    { 0x87, "STA [d]",     2, 2, false },
    { 0x88, "DEY i",       2, 1, true  },
    { 0x89, "BIT #",       2, 2, true  },
    { 0x8A, "TXA i",       2, 1, true  },
    { 0x8B, "PHB s",       3, 1, false },
    { 0x8C, "STY a",       4, 3, true  },
    { 0x8D, "STA a",       4, 3, true  },
    { 0x8E, "STX a",       4, 3, true  },
    { 0x8F, "STA al",      5, 4, false },
    { 0x90, "BCC r",       2, 2, true  },
    { 0x91, "STA (d),y",   6, 2, true  },
    { 0x92, "STA (d)",     5, 2, true  },
    { 0x93, "STA (d,s),y", 7, 2, false },
    { 0x94, "STY d,x",     4, 2, true  },
    { 0x95, "STA d,x",     4, 2, true  },
    { 0x96, "STX d,y",     4, 2, true  },
    { 0x97, "STA [d],y",   6, 2, false },
    { 0x98, "TYA i",       2, 1, true  },
    { 0x99, "STA a,y",     5, 3, true  },
    { 0x9A, "TXS i",       2, 1, true  },
    { 0x9B, "TXY i",       2, 1, false },
    { 0x9C, "STZ a",       4, 3, true  },
    { 0x9D, "STA a,x",     5, 3, true  },
    { 0x9E, "STZ a,x",     5, 3, true  },
    { 0x9F, "STA al,x",    5, 4, false },
    { 0xA0, "LDY #",       2, 2, true  },
    { 0xA1, "LDA (d,x)",   6, 2, true  },
    { 0xA2, "LDX #",       2, 2, false },
    { 0xA3, "LDA d,s",     4, 2, false },
    { 0xA4, "LDY d",       3, 2, true  },
    { 0xA5, "LDA d",       3, 2, true  },
    { 0xA6, "LDX d",       3, 2, true  },
    { 0xA7, "LDA [d]",     6, 2, false },
    { 0xA8, "TAY i",       2, 1, true  },
    { 0xA9, "LDA #",       2, 2, true  },
    { 0xAA, "TAX i",       2, 1, true  },
    { 0xAB, "PLB s",       4, 1, false },
    { 0xAC, "LDY a",       4, 3, true  },
    { 0xAD, "LDA a",       4, 3, true  },
    { 0xAE, "LDX a",       4, 3, true  },
    { 0xAF, "LDA al",      5, 4, false },
    { 0xB0, "BCS r",       2, 2, true  },
    { 0xB1, "LDA (d),y",   5, 2, true  },
    { 0xB2, "LDA (d)",     5, 2, true  },
    { 0xB3, "LDA (d,s),y", 7, 2, false },
    { 0xB4, "LDY d,x",     4, 2, true  },
    { 0xB5, "LDA d,x",     4, 2, true  },
    { 0xB6, "LDX d,y",     4, 2, true  },
    { 0xB7, "LDA [d],y",   6, 2, false },
    { 0xB8, "CLV i",       2, 1, true  },
    { 0xB9, "LDA a,y",     4, 3, true  },
    { 0xBA, "TSX i",       2, 1, true  },
    { 0xBB, "TYX i",       2, 1, false },
    { 0xBC, "LDY a,x",     4, 3, true  },
    { 0xBD, "LDA a,x",     4, 3, true  },
    { 0xBE, "LDX a,y",     4, 3, true  },
    { 0xBF, "LDA al,x",    5, 4, false },
    { 0xC0, "CPY #",       2, 2, true  },
    { 0xC1, "CMP (d,x)",   6, 2, true  },
    { 0xC2, "REP #",       3, 2, false },
    { 0xC3, "CMP d,s",     4, 2, false },
    { 0xC4, "CPY d",       3, 2, true  },
    { 0xC5, "CMP d",       3, 2, true  },
    { 0xC6, "DEC d",       5, 2, true  },
    { 0xC7, "CMP [d]",     6, 2, false },
    { 0xC8, "INY i",       2, 1, true  },
    { 0xC9, "CMP #",       2, 2, true  },
    { 0xCA, "DEX i",       2, 1, true  },
    { 0xCB, "WAI i",       3, 1, true  },
    { 0xCC, "CPY a",       4, 3, true  },
    { 0xCD, "CMP a",       4, 3, true  },
    { 0xCE, "DEC a",       6, 3, true  },
    { 0xCF, "CMP al",      5, 4, false },
    { 0xD0, "BNE r",       2, 2, true  },
    { 0xD1, "CMP (d),y",   5, 2, true  },
    { 0xD2, "CMP (d)",     5, 2, true  },
    { 0xD3, "CMP (d,s),y", 7, 2, false },
    { 0xD4, "PEI s",       6, 2, false },
    { 0xD5, "CMP d,x",     4, 2, true  },
    { 0xD6, "DEC d,x",     6, 2, true  },
    { 0xD7, "CMP [d],y",   6, 2, false },
    { 0xD8, "CLD i",       2, 1, true  },
    { 0xD9, "CMP a,y",     4, 3, true  },
    { 0xDA, "PHX s",       3, 1, true  },
    { 0xDB, "STP i",       3, 1, true  },
    { 0xDC, "JML (a)",     6, 3, false },
    { 0xDD, "CMP a,x",     4, 3, true  },
    { 0xDE, "DEC a,x",     7, 3, true  },
    { 0xDF, "CMP al,x",    5, 4, false },
    { 0xE0, "CPX #",       2, 2, true  },
    { 0xE1, "SBC (d,x)",   6, 2, true  },
    { 0xE2, "SEP #",       3, 2, false },
    { 0xE3, "SBC d,s",     4, 2, false },
    { 0xE4, "CPX d",       3, 2, true  },
    { 0xE5, "SBC d",       3, 2, true  },
    { 0xE6, "INC d",       5, 2, true  },
    { 0xE7, "SBC [d]",     6, 2, false },
    { 0xE8, "INX i",       2, 1, true  },
    { 0xE9, "SBC #",       2, 2, true  },
    { 0xEA, "NOP i",       2, 1, true  },
    { 0xEB, "XBA i",       3, 1, false },
    { 0xEC, "CPX a",       4, 3, true  },
    { 0xED, "SBC a",       4, 3, true  },
    { 0xEE, "INC a",       6, 3, true  },
    { 0xEF, "SBC al",      5, 4, false },
    { 0xF0, "BEQ r",       2, 2, true  },
    { 0xF1, "SBC (d),y",   5, 2, true  },
    { 0xF2, "SBC (d)",     5, 2, true  },
    { 0xF3, "SBC (d,s),y", 7, 2, false },
    { 0xF4, "PEA s",       5, 3, false },
    { 0xF5, "SBC d,x",     4, 2, true  },
    { 0xF6, "INC d,x",     6, 2, true  },
    { 0xF7, "SBC [d],y",   6, 2, false },
    { 0xF8, "SED i",       2, 1, true  },
    { 0xF9, "SBC a,y",     4, 3, true  },
    { 0xFA, "PLX s",       4, 1, true  },
    { 0xFB, "XCE i",       2, 1, false },
    { 0xFC, "JSR (a,x)",   8, 3, true  },
    { 0xFD, "SBC a,x",     4, 3, true  },
    { 0xFE, "INC a,x",     7, 3, true  },
    { 0xFF, "SBC al,x",    5, 4, false },
};

// Skip failing tests for unimplemented instructions.
const int UNIMPL_skip[] = {
    0x04, 0x0C, 0x14, 0x1C, 0x5A, 0x7A, 0xCB, 0xDA, 0xFA,  //
    0x64, 0x74, 0x92, 0xB2,
};

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
    }
}
