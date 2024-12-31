#include "./args.h"

#include <argp.h>

#define BUGS_ADDRESS "https://github.com/X65/emu/issues"
const char* app_bug_address = BUGS_ADDRESS;
const char* app_releases_address = "https://github.com/X65/emu/releases";
#define FULL_NAME "X65 microcomputer emulator"
const char full_name[] = FULL_NAME;

static char args_doc[] = "[ROM.xex]";
static struct argp_option options[] = {
    { "verbose", 'v', 0, 0, "Produce verbose output" },
    { "quiet", 'q', 0, 0, "Don't produce any output" },
    { "silent", 's', 0, OPTION_ALIAS },
    { "output", 'o', "FILE", 0, "Output to FILE instead of standard output" },
    { "ini", 'i', ".INI", 0, ".ini file used to store window position" },
    { 0 }
};

struct arguments arguments = { NULL, 0, 0, "-" };

void args_dump(void) {
    printf(
        "ROM = %s\nOUTPUT_FILE = %s\n"
        "VERBOSE = %s\nSILENT = %s\nINI_FILE = %s\n",
        arguments.rom,
        arguments.output_file,
        arguments.verbose ? "yes" : "no",
        arguments.silent ? "yes" : "no",
        arguments.ini_file);
}

static error_t parse_opt(int key, char* arg, struct argp_state* argp_state) {
    struct arguments* args = argp_state->input;

    switch (key) {
        case 'q':
        case 's': args->silent = 1; break;
        case 'v': args->verbose = 1; break;
        case 'o': args->output_file = arg; break;
        case 'i': args->ini_file = arg; break;

        case ARGP_KEY_ARG:
            if (argp_state->arg_num >= 2) /* Too many arguments. */
                argp_usage(argp_state);

            args->rom = arg;
            break;

        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options,
                            parse_opt,
                            args_doc,
                            FULL_NAME
                            "\v"
                            "Report bugs to: " BUGS_ADDRESS };

void args_parse(int argc, char* argv[]) {
    argp_program_version = program_version;
    argp_parse(&argp, argc, argv, 0, 0, &arguments);
}
