#include "./args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPTPARSE_IMPLEMENTATION
#include <optparse/optparse.h>

#define BUGS_ADDRESS "https://github.com/X65/emu/issues"
const char* app_bug_address = BUGS_ADDRESS;
const char* app_releases_address = "https://github.com/X65/emu/releases";
#define FULL_NAME "X65 microcomputer emulator"
const char full_name[] = FULL_NAME;

struct arguments arguments = {
    .output_file = "-",
};
static char args_doc[] = "[ROM.xex]";

enum { OPT_ALIAS = 1 << 0, OPT_ARG_OPTIONAL = 1 << 1 };

// Keys for long-only options (no short form); must stay above ASCII range.
enum {
    KEY_DISABLE_SPEAKER_ICON = 0x100,
};

typedef struct {
    const char* name;  // long option; NULL terminates the table
    int key;           // short option char
    const char* arg;   // arg placeholder (e.g. "FILE"); NULL if no argument
    int flags;         // OPT_ALIAS | OPT_ARG_OPTIONAL
    const char* doc;   // help text; NULL for an alias row
} option_t;

static const option_t options[] = {
    { "verbose",              'v',                      NULL,          0,         "Produce verbose output"                                                },
    { "quiet",                'q',                      NULL,          0,         "Don't produce any output"                                              },
    { "silent",               's',                      NULL,          OPT_ALIAS, NULL                                                                    },
    { "output",               'o',                      "FILE",        0,         "Output to FILE instead of standard output"                             },
    { "labels",               'l',                      "LABELS_FILE", 0,         "Load VICE compatible global labels file"                               },
    { "joystick",
     'j',                                               "TYPE",
     OPT_ARG_OPTIONAL,                                                            "Enable joystick; TYPE is digital_1 (default), digital_2 or digital_12" },
    { "zero-mem",             'z',                      NULL,          0,         "Fill memory with zeros"                                                },
    { "dap",                  'd',                      NULL,          0,         "Enable Debug Adapter Protocol over stdin/stdout"                       },
    { "dap-port",             'p',                      "PORT",        0,         "Enable Debug Adapter Protocol over TCP port"                           },
    { "crt",
     'c',                                               "VALUES",
     OPT_ARG_OPTIONAL,                                                            "Enable CRT post-process effect; optional VALUES is a comma-separated "
      "list of up to 6 floats: scanlines,mask,curvature,vignette,blur,gamma "
      "(empty positions keep current values)"                                                                                       },
    { "fullscreen",           'f',                      NULL,          0,         "Start in fullscreen mode"                                              },
    { "disable-gui",          'g',                      NULL,          0,         "Start with debug UI hidden"                                            },
    { "disable-speaker-icon", KEY_DISABLE_SPEAKER_ICON, NULL,          0,         "Hide the speaker status icon"                                          },
    { "break",                'b',                      "OPCODE",      0,         "Break on hex OPCODE (e.g. 00 or EA)"                                   },
    { NULL,                   0,                        NULL,          0,         NULL                                                                    },
};

// Layout of the --help option list.
#define HELP_DOC_COLUMN 29  // column where doc text begins
#define HELP_LINE_WIDTH 79  // total line width used for word-wrapping

static void print_usage(FILE* f) {
    fprintf(f, "Usage: emu [OPTION...] %s\n", args_doc);
}

static bool key_is_short(int key) {
    return key > 0 && key < 128;
}

// Append "-x, --name[=ARG]" (or "    --name" for a long-only option) onto the
// left-column buffer.
static void help_append_names(char* buf, size_t size, const option_t* opt) {
    size_t len = strlen(buf);
    if (len) {
        // separate from a previously appended (aliased) option
        snprintf(buf + len, size - len, ", ");
        len = strlen(buf);
    }
    if (key_is_short(opt->key)) {
        snprintf(buf + len, size - len, "-%c, --%s", opt->key, opt->name);
    }
    else {
        // no short form: pad so "--name" aligns under short options
        snprintf(buf + len, size - len, "    --%s", opt->name);
    }
    len = strlen(buf);
    if (opt->arg) {
        if (opt->flags & OPT_ARG_OPTIONAL) {
            snprintf(buf + len, size - len, "[=%s]", opt->arg);
        }
        else {
            snprintf(buf + len, size - len, "=%s", opt->arg);
        }
    }
}

// Print one help entry: the left column (names) padded, then the word-wrapped doc.
static void help_print_entry(const char* names, const char* doc) {
    fputs("  ", stdout);
    int col = 2 + (int)strlen(names);
    fputs(names, stdout);

    if (!doc || !doc[0]) {
        fputc('\n', stdout);
        return;
    }

    // Align to the doc column; wrap to the next line if names are too long.
    if (col + 1 > HELP_DOC_COLUMN) {
        fputc('\n', stdout);
        col = 0;
    }
    for (; col < HELP_DOC_COLUMN; col++)
        fputc(' ', stdout);

    // Greedy word wrap of the doc text into the doc column.
    const char* w = doc;
    int written = col;
    bool first = true;
    while (*w) {
        while (*w == ' ')
            w++;
        if (!*w) break;
        const char* end = w;
        while (*end && *end != ' ')
            end++;
        int wl = (int)(end - w);
        if (!first && written + 1 + wl > HELP_LINE_WIDTH) {
            fputc('\n', stdout);
            for (written = 0; written < HELP_DOC_COLUMN; written++)
                fputc(' ', stdout);
        }
        else if (!first) {
            fputc(' ', stdout);
            written++;
        }
        fwrite(w, 1, (size_t)wl, stdout);
        written += wl;
        first = false;
        w = end;
    }
    fputc('\n', stdout);
}

static void print_help(void) {
    print_usage(stdout);
    printf("%s\n\n", full_name);

    for (const option_t* opt = options; opt->name; opt++) {
        if (opt->flags & OPT_ALIAS) continue;  // handled with its primary option

        char names[128] = { 0 };
        help_append_names(names, sizeof(names), opt);
        // Fold any following alias rows onto the same line.
        const option_t* next = opt + 1;
        while (next->name && (next->flags & OPT_ALIAS)) {
            help_append_names(names, sizeof(names), next);
            next++;
        }
        help_print_entry(names, opt->doc);
    }

    help_print_entry("-h, --help", "Give this help list");
    help_print_entry("-V, --version", "Print program version");

    printf("\nReport bugs to: %s\n", app_bug_address);
}

void args_parse(int argc, char* argv[]) {
    (void)argc;

    // Build the optparse long-option table from our single source of truth,
    // plus the synthetic --help/--version entries argp-like.
    struct optparse_long longopts[sizeof(options) / sizeof(options[0]) + 2];
    int n = 0;
    for (const option_t* opt = options; opt->name; opt++) {
        longopts[n].longname = opt->name;
        longopts[n].shortname = opt->key;
        longopts[n].argtype =
            opt->arg ? ((opt->flags & OPT_ARG_OPTIONAL) ? OPTPARSE_OPTIONAL : OPTPARSE_REQUIRED) : OPTPARSE_NONE;
        n++;
    }
    longopts[n++] = (struct optparse_long){ "help", 'h', OPTPARSE_NONE };
    longopts[n++] = (struct optparse_long){ "version", 'V', OPTPARSE_NONE };
    longopts[n++] = (struct optparse_long){ 0 };

    struct optparse opt;
    optparse_init(&opt, argv);

    int key;
    while ((key = optparse_long(&opt, longopts, NULL)) != -1) {
        switch (key) {
            case 'q':
            case 's': arguments.silent = true; break;
            case 'v': arguments.verbose = true; break;
            case 'j': arguments.joystick = opt.optarg ? opt.optarg : "digital_1"; break;
            case 'z': arguments.zeromem = true; break;
            case 'o': arguments.output_file = opt.optarg; break;
            case 'd': arguments.dap = true; break;
            case 'p': arguments.dap_port = opt.optarg; break;
            case 'c':
                arguments.crt = true;
                if (opt.optarg && opt.optarg[0]) {
                    arguments.crt_values = opt.optarg;
                }
                break;
            case 'f': arguments.fullscreen = true; break;
            case 'g': arguments.disable_gui = true; break;
            case 'b': arguments.break_opcode = opt.optarg; break;

            case KEY_DISABLE_SPEAKER_ICON: arguments.disable_speaker_icon = true; break;

            case 'l': app_load_labels(opt.optarg, false); break;

            case 'h': print_help(); exit(EXIT_SUCCESS);
            case 'V': printf("%s\n", program_version); exit(EXIT_SUCCESS);

            case '?':
                fprintf(stderr, "%s: %s\n", app_name, opt.errmsg);
                fprintf(stderr, "Try 'emu --help' for more information.\n");
                exit(EXIT_FAILURE);
        }
    }

    const char* pos;
    bool rom_set = false;
    while ((pos = optparse_arg(&opt))) {
        if (rom_set) {
            fprintf(stderr, "%s: too many arguments\n", app_name);
            print_usage(stderr);
            exit(EXIT_FAILURE);
        }
        arguments.rom = pos;
        rom_set = true;
    }
}
