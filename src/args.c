#include "./args.h"

extern void app_load_labels(const char* file);

#ifdef USE_ARGP
    #include <argp.h>
#endif
#include <sokol_args.h>

#define BUGS_ADDRESS "https://github.com/X65/emu/issues"
const char* app_bug_address = BUGS_ADDRESS;
const char* app_releases_address = "https://github.com/X65/emu/releases";
#define FULL_NAME "X65 microcomputer emulator"
const char full_name[] = FULL_NAME;

struct arguments arguments = { NULL, 0, 0, "-" };
static char args_doc[] = "[ROM.xex]";

#ifdef USE_ARGP
static struct argp_option options[] = {
    { "verbose", 'v', 0, 0, "Produce verbose output" },
    { "quiet", 'q', 0, 0, "Don't produce any output" },
    { "silent", 's', 0, OPTION_ALIAS },
    { "output", 'o', "FILE", 0, "Output to FILE instead of standard output" },
    { "labels", 'l', "LABELS_FILE", 0, "Load VICE compatible global labels file" },
    { 0 }
};

static error_t parse_opt(int key, char* arg, struct argp_state* argp_state) {
    struct arguments* args = argp_state->input;

    switch (key) {
        case 'q':
        case 's': args->silent = 1; break;
        case 'v': args->verbose = 1; break;
        case 'o': args->output_file = arg; break;

        case 'l': app_load_labels(arg); break;

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
#endif

void args_parse(int argc, char* argv[]) {
#ifdef USE_ARGP
    argp_program_version = program_version;
    argp_parse(&argp, argc, argv, 0, NULL, &arguments);
#endif

    if (sargs_exists("file")) {
        arguments.rom = sargs_value("file");
    }
}
