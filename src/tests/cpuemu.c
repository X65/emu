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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

// 16 MBytes of memory
uint8_t mem[1 << 24] = { 0 };

#define BUGS_ADDRESS "https://github.com/X65/emu/issues"

static char args_doc[] = "[file.bin ...]";
static struct argp_option options[] = {
    { "addr", 'a', "HEX", 0, "Load binary file at address" },
    { "reset", 'r', "HEX", 0, "Set reset vector" },
    { "dump", 'd', "HEX", 0, "Print memory value before exit" },
    { "quiet", 'q', 0, 0, "Don't produce output" },
    { "silent", 's', 0, OPTION_ALIAS },
    { "verbose", 'v', 0, 0, "Produce output" },
    { "input", 'i', "HEX", 0, "Serial input port" },
    { "output", 'o', "HEX", 0, "Serial output port" },
    { "crlf", 'l', 0, 0, "Convert input LF to CRLF" },
    { "write", 'w', "FILE", 0, "Write output to file" },
    { "uart", 'u', 0, 0, "Emulate X65 CDC-UART" },
    { "no-brk", 'b', 0, 0, "Do not stop on BRK instruction" },
    { 0 }
};

struct arguments {
    int silent, dump, input, output, crlf, uart, nbrk;
    char* write;
} arguments = { 0, -1, -1, -1, 0, 0, 0, NULL };

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
        case 'v': args->silent = 0; break;

        case 'a': load_addr = (uint16_t)strtoul(arg, NULL, 16); break;
        case 'r':
            uint16_t reset_addr = (uint16_t)strtoul(arg, NULL, 16);
            mem[0xFFFC] = reset_addr & 0xFF;
            mem[0xFFFD] = (reset_addr >> 8) & 0xFF;
            break;
        case ARGP_KEY_ARG: load_bin(arg, load_addr); break;

        case 'd': args->dump = (uint16_t)strtoul(arg, NULL, 16); break;
        case 'w': args->write = strdup(arg); break;

        case 'i':
            args->input = (uint16_t)strtoul(arg, NULL, 16);
            args->silent = true;
            break;
        case 'o':
            args->output = (uint16_t)strtoul(arg, NULL, 16);
            args->silent = true;
            break;
        case 'l': args->crlf = 1; break;
        case 'u':
            args->uart = 1;
            args->silent = true;
            break;

        case 'b': args->nbrk = 1; break;

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

static struct termios ttysave;

static char pending_char = 0;

void cli_init() {
    tcgetattr(STDIN_FILENO, &ttysave);
    // do not wait for ENTER
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~(ICANON | ECHO);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tty);

    // set STDIN to non-blocking I/O
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

void cli_cleanup(int _status, void* _arg) {
    (void)_status;
    (void)_arg;
    // restore terminal settings
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttysave);
}

FILE* output = NULL;

void __attribute__((__noreturn__)) exit_with_message(const char* message) {
    fprintf(stderr, "%s\n", message);
    if (arguments.dump >= 0 && arguments.dump < sizeof(mem)) {
        fprintf(output, "%02X\n", mem[arguments.dump]);
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    args_parse(argc, argv);

    output = stdout;

    if (arguments.input >= 0 || arguments.uart) {
        cli_init();
        on_exit(cli_cleanup, NULL);
        output = stderr;
    }
    if (arguments.write && arguments.write[0]) {
        output = fopen(arguments.write, "wb");
        if (!output) {
            fprintf(stderr, "Error: can't open file %s for writing\n", arguments.write);
            exit(1);
        }
    }

    // initialize the CPU
    w65816_t cpu;
    uint64_t pins = w65816_init(&cpu, &(w65816_desc_t){});

    while (true) {
        static char in_c = '\0';
        static ssize_t in_n = 0;
        if (in_n == 0 && (arguments.input >= 0 || arguments.uart)) {
            in_n = read(STDIN_FILENO, &in_c, 1);
        }

        // run the CPU emulation for one tick
        pins = w65816_tick(&cpu, pins);
        // extract 24-bit address from pin mask
        const uint32_t addr = W65816_GET_ADDR(pins);
        // is this a read?
        const bool cpu_read = pins & W65816_RW;
        // perform memory access
        if (cpu_read) {
            // a memory read
            uint8_t data = mem[addr];

            if (arguments.input >= 0 && addr == arguments.input) {
                data = 0x00;  // by default read "nothing"

                if (pending_char) {
                    data = pending_char;
                    pending_char = 0;
                }
                else if (in_n > 0) {
                    if (arguments.crlf && in_c == 0x0A) {
                        // convert LF to CRLF
                        pending_char = in_c;  // put away LF for later
                        in_c = 0x0D;          // insert CR
                    }
                    data = in_c;
                    in_n = 0;
                }
            }
            if (arguments.uart) {
                if (addr == 0xffe0) {
                    // FLOW control
                    data = 0b10000000;
                    if (in_n > 0) {
                        data |= 0b01000000;
                        in_n = 0;
                    };
                }
                if (addr == 0xffe1) {
                    // RX
                    data = in_c;
                    in_n = 0;
                }
            }

            W65816_SET_DATA(pins, data);
        }
        else {
            // a memory write
            uint8_t data = W65816_GET_DATA(pins);
            if ((arguments.output >= 0 && addr == arguments.output) || (arguments.uart && addr == 0xffe1)) {
                putc(data, stdout);
                fflush(stdout);
            }
            else {
                mem[addr] = data;
            }
        }

        uint8_t data = W65816_GET_DATA(pins);
        // reading a new instruction
        if (cpu_read && (pins & W65816_VPA) && (pins & W65816_VDA)) {
            // handle special cases
            switch (data) {
                case 0x00:  // BRK
                    if (arguments.nbrk) break;
                    exit_with_message("BRK instruction reached");
                case 0xCB:  // WAI
                    exit_with_message("WAI instruction reached");
                case 0xDB:  // STP
                    exit_with_message("STP instruction reached");
            }
            // exit on infinite loop
            static uint32_t last_addr = 0;
            static uint8_t last_instr = 0;
            if (last_addr == addr && last_instr != 0x60 /* RTS */) {
                exit_with_message("Infinite loop detected");
            }
            last_addr = addr;
            last_instr = data;

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
            fprintf(
                output,
                "%s%s%s  ADDR: %02X %04X  DATA: %02X\t\tPC: %02X %04X  C: %04X  X: %04X  Y: %04X  SP: %04X  DB: %02X",
                cpu_read ? "R" : "w",
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
                fprintf(output, "\t%s\n", dasm_buffer);
            }
            else {
                fprintf(output, "\n");
            }
        }
    }
}
