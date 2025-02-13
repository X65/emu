/**
 * This is a standalone CPU Simulator inspired by this article:
 * https://codegolf.stackexchange.com/questions/12844/emulate-a-mos-6502-cpu
 *
 * You can use it to run test binaries in headless mode.
 * i.e.:
 *     build/src/tests/cpuemu -a 4000 src/tests/AllSuiteA.bin -r 4000 -d 0210 -s 2>/dev/null
 *     FF
 */

#define CHIPS_IMPL
#include "chips/w65c816s.h"
#define CHIPS_UTIL_IMPL
#include "util/w65c816sdasm.h"

#include <argp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// 16 MBytes of memory
uint8_t mem[1 << 24] = { 0 };

#define BUGS_ADDRESS "https://github.com/X65/emu/issues"

static char args_doc[] = "[file.bin ...]";
static struct argp_option options[] = {
    { "addr", 'a', "HEX", 0, "Load binary file at address" },
    { "reset", 'r', "HEX", 0, "Set reset vector" },
    { "dump", 'd', "HEX", 0, "Print memory value before exit" },
    { "verbose", 'v', 0, 0, "Produce verbose output" },
    { "quiet", 'q', 0, 0, "Don't produce any output" },
    { "silent", 's', 0, OPTION_ALIAS },
    { 0 }
};

struct arguments {
    int silent, verbose, dump;
} arguments = { 0, 0, -1 };

uint16_t load_addr = 0;

static void load_bin(const char* filename, uint16_t addr) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: can't open file %s\n", filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > 0xFFFF) {
        fprintf(stderr, "Error: file %s is too large\n", filename);
        exit(1);
    }
    fread(mem + addr, 1, size, f);
    fclose(f);
}

static error_t parse_opt(int key, char* arg, struct argp_state* argp_state) {
    struct arguments* args = argp_state->input;

    switch (key) {
        case 'q':
        case 's': args->silent = 1; break;
        case 'v': args->verbose = 1; break;

        case 'a': load_addr = (uint16_t)strtoul(arg, NULL, 16); break;
        case 'r':
            uint16_t reset_addr = (uint16_t)strtoul(arg, NULL, 16);
            mem[0xFFFC] = reset_addr & 0xFF;
            mem[0xFFFD] = (reset_addr >> 8) & 0xFF;
            break;
        case 'd': args->dump = (uint16_t)strtoul(arg, NULL, 16); break;

        case ARGP_KEY_ARG: load_bin(arg, load_addr); break;

        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options,
                            parse_opt,
                            args_doc,
                            "W65C816S CPU simulator"
                            "\v"
                            "Report bugs to: " BUGS_ADDRESS };

void args_parse(int argc, char* argv[]) {
    argp_parse(&argp, argc, argv, 0, NULL, &arguments);
}

static char dasm_buffer[256];
static struct dasm_context_t {
    uint8_t* mem;
    char* buffer;
} dasm_context;
static uint8_t dasm_in_cb(void* user_data) {
    struct dasm_context_t* ctx = (struct dasm_context_t*)user_data;
    return *ctx->mem++;
}
static void dasm_out_cb(char c, void* user_data) {
    struct dasm_context_t* ctx = (struct dasm_context_t*)user_data;
    *ctx->buffer++ = c;
}

void __attribute__((__noreturn__)) exit_with_message(const char* message) {
    fprintf(stderr, "%s\n", message);
    if (arguments.dump >= 0 && arguments.dump < sizeof(mem)) {
        printf("%02X\n", mem[arguments.dump]);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    args_parse(argc, argv);

    // initialize the CPU
    w65816_t cpu;
    uint64_t pins = w65816_init(&cpu, &(w65816_desc_t){});

    while (true) {
        // run the CPU emulation for one tick
        pins = w65816_tick(&cpu, pins);
        // extract 24-bit address from pin mask
        const uint32_t addr = W65816_GET_ADDR(pins);
        // is this a read?
        const bool read = pins & W65816_RW;
        // perform memory access
        if (read) {
            // a memory read
            W65816_SET_DATA(pins, mem[addr]);
        }
        else {
            // a memory write
            mem[addr] = W65816_GET_DATA(pins);
        }

        uint8_t data = W65816_GET_DATA(pins);
        // reading a new instruction
        if (read && (pins & W65816_VPA) && (pins & W65816_VDA)) {
            // handle special cases
            switch (data) {
                case 0x00:  // BRK
                    exit_with_message("BRK instruction reached");
                case 0xCB:  // WAI
                    exit_with_message("WAI instruction reached");
                case 0xDB:  // STP
                    exit_with_message("STP instruction reached");
            }
            // exit on infinite loop
            static uint32_t last_addr = 0;
            if (last_addr == addr) {
                exit_with_message("Infinite loop detected");
            }
            last_addr = addr;

            // disassemble the instruction
            dasm_context.mem = mem + addr;
            dasm_context.buffer = dasm_buffer;
            memset(dasm_buffer, 0, sizeof(dasm_buffer));
            w65816dasm_op(addr, dasm_in_cb, dasm_out_cb, &dasm_context);
        }
        else {
            dasm_buffer[0] = '\0';
        }

        if (!arguments.silent) {
            // print the current state
            printf(
                "%s%s%s  ADDR: %02X %04X  DATA: %02X\t\tPC: %02X %04X  C: %04X  X: %04X  Y: %04X  SP: %04X  DB: %02X",
                read ? "R" : "w",
                (pins & W65816_VPA) ? "P" : " ",
                (pins & W65816_VDA) ? "D" : " ",
                W65816_GET_BANK(pins),
                addr,
                data,
                w65816_pb(&cpu),
                w65816_pc(&cpu),
                w65816_c(&cpu),
                w65816_x(&cpu),
                w65816_y(&cpu),
                w65816_s(&cpu),
                w65816_db(&cpu));
            if (dasm_buffer[0] != '\0') {
                printf("\t%s\n", dasm_buffer);
            }
            else {
                printf("\n");
            }
        }
    }
}
