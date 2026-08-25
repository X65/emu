#include "./args.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPTPARSE_IMPLEMENTATION
#include <optparse/optparse.h>

#define BUGS_ADDRESS "https://github.com/X65/emu/issues"
const char* app_bug_address = BUGS_ADDRESS;
const char* app_releases_address = "https://github.com/X65/emu/releases";
#define SHORT_NAME "emu"
#define FULL_NAME  "X65 microcomputer emulator"
const char full_name[] = FULL_NAME;

// Single source of truth for default argument values, shared by the process
// global and args_defaults() so the two can't drift apart.
#define ARGUMENTS_DEFAULTS { .output_file = "-" }

struct arguments arguments = ARGUMENTS_DEFAULTS;

struct arguments args_defaults(void) {
    return (struct arguments)ARGUMENTS_DEFAULTS;
}

static char args_doc[] = "[ROM.xex]";

enum { OPT_ALIAS = 1 << 0, OPT_ARG_OPTIONAL = 1 << 1 };

// Keys for long-only options (no short form); must stay above ASCII range.
enum {
    KEY_DISABLE_SPEAKER_ICON = 0x100,
    KEY_SGU_DUMP,
    KEY_SCRIPT,
    KEY_SCREENSHOT,
    KEY_FRAMES,
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
    { "sgu-dump",             KEY_SGU_DUMP,             "FILE",        0,         "Dump SGU-1 registers to FILE at end of each frame that has register writes" },
    { "script",               KEY_SCRIPT,               "FILE",        0,         "Run a headless test/debug script ('-' = stdin); see src/script.h for the verbs" },
    { "screenshot",           KEY_SCREENSHOT,           "FILE",        0,         "Write a PNG of the display after --frames frames and exit"           },
    { "frames",               KEY_FRAMES,               "N",           0,         "Frames to run before --screenshot (default 120)"                       },
    { NULL,                   0,                        NULL,          0,         NULL                                                                    },
};

// Layout of the --help option list.
#define HELP_DOC_COLUMN 29  // column where doc text begins
#define HELP_LINE_WIDTH 79  // total line width used for word-wrapping

static void print_usage(FILE* f) {
    fprintf(f, "Usage: " SHORT_NAME " [OPTION...] %s\n", args_doc);
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

// Decode %XX escapes and '+' in place (application/x-www-form-urlencoded style).
static void url_decode(char* s) {
    char* o = s;
    for (char* p = s; *p;) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            char hex[3] = { p[1], p[2], 0 };
            *o++ = (char)strtol(hex, NULL, 16);
            p += 3;
        }
        else if (*p == '+') {
            *o++ = ' ';
            p++;
        }
        else {
            *o++ = *p++;
        }
    }
    *o = '\0';
}

// Convert a URL query string into an argv array parseable by optparse:
//   ?fullscreen&crt=1,2,3&file=rom.xex -> { prog, "--fullscreen", "--crt=1,2,3", "rom.xex", NULL }
// Rules: an optional leading '?' is skipped; split on '&'; "file=VALUE" becomes
// a bare positional (may repeat, for many-files support); every other token that
// isn't already "--..." is given a "--" prefix (only the long "--option[=value]"
// form is supported).
char** args_build_argv_from_query(const char* prog, const char* query) {
    const char* search = query ? query : "";
    if (search[0] == '?') search++;
    char* buf = strdup(search);

    int cap = 8, argc = 0;
    char** argv = malloc((size_t)cap * sizeof(char*));
#define PUSH(S)                                                \
    do {                                                       \
        if (argc + 1 >= cap) {                                 \
            cap *= 2;                                          \
            argv = realloc(argv, (size_t)cap * sizeof(char*)); \
        }                                                      \
        argv[argc++] = (S);                                    \
    } while (0)

    PUSH((char*)(prog ? prog : SHORT_NAME));  // argv[0] is the program name

    char* p = buf;
    while (*p) {
        char* tok = p;
        char* amp = strchr(p, '&');
        if (amp) {
            *amp = '\0';
            p = amp + 1;
        }
        else {
            p += strlen(p);
        }
        if (!tok[0]) continue;
        url_decode(tok);

        if (strncmp(tok, "file=", 5) == 0) {
            if (tok[5]) PUSH(tok + 5);  // positional file argument
        }
        else if (strncmp(tok, "--", 2) == 0) {
            PUSH(tok);  // already in long form
        }
        else {
            size_t n = strlen(tok) + 3;
            char* opt = malloc(n);
            snprintf(opt, n, "--%s", tok);
            PUSH(opt);
        }
    }
    PUSH(NULL);  // optparse expects a NULL-terminated argv
#undef PUSH

    return argv;
}

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>

    // keep the JS runtime helpers used by the EM_JS below alive through closure
    #if defined(EM_JS_DEPS)
EM_JS_DEPS(x65args, "$withStackSave,$stringToUTF8OnStack");
    #endif

// The page URL query string, captured from JS into a C string.
static char* web_query = NULL;

EMSCRIPTEN_KEEPALIVE void args_store_query(const char* q) {
    web_query = strdup(q ? q : "");
}

// Read window.location.search and hand it to args_store_query() as a C string.
// clang-format off
EM_JS(void, args_js_read_query, (void), {
    var search = window.location.search || "";
    withStackSave(function() { _args_store_query(stringToUTF8OnStack(search)); });
})
// clang-format on

// On the web there is no command line; capture the page URL query and turn it
// into an argv for the normal parser.
static char** web_build_argv(char* prog) {
    args_js_read_query();
    return args_build_argv_from_query(prog, web_query);
}
#endif  // __EMSCRIPTEN__

args_status_t args_parse_argv(char** argv, struct arguments* out, const char** errmsg) {
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
            case 's': out->silent = true; break;
            case 'v': out->verbose = true; break;
            case 'j': out->joystick = opt.optarg ? opt.optarg : "digital_1"; break;
            case 'z': out->zeromem = true; break;
            case 'o': out->output_file = opt.optarg; break;
            case 'd': out->dap = true; break;
            case 'p': out->dap_port = opt.optarg; break;
            case 'c':
                out->crt = true;
                if (opt.optarg && opt.optarg[0]) {
                    out->crt_values = opt.optarg;
                }
                break;
            case 'f': out->fullscreen = true; break;
            case 'g': out->disable_gui = true; break;
            case 'b': out->break_opcode = opt.optarg; break;

            case KEY_DISABLE_SPEAKER_ICON: out->disable_speaker_icon = true; break;
            case KEY_SGU_DUMP: out->sgu_dump = opt.optarg; break;
            case KEY_SCRIPT: out->script = opt.optarg; break;
            case KEY_SCREENSHOT: out->screenshot = opt.optarg; break;
            case KEY_FRAMES: out->frames = opt.optarg; break;

            case 'l': app_load_labels(opt.optarg, false); break;

            case 'h': return ARGS_SHOW_HELP;
            case 'V': return ARGS_SHOW_VERSION;

            case '?':
                if (errmsg) {
                    // opt.errmsg lives in this frame's `opt`; copy it somewhere
                    // that outlives the return so the caller can print it.
                    static char errbuf[sizeof(opt.errmsg)];
                    snprintf(errbuf, sizeof(errbuf), "%s", opt.errmsg);
                    *errmsg = errbuf;
                }
                return ARGS_ERROR;
        }
    }

    const char* pos;
    while ((pos = optparse_arg(&opt))) {
        if (!out->rom) out->rom = pos;  // first positional file
    }
    return ARGS_OK;
}

void args_parse(int argc, char* argv[]) {
    (void)argc;

#ifdef __EMSCRIPTEN__
    // On the web there is no command line; arguments come from the page URL.
    argv = web_build_argv(argc > 0 ? argv[0] : NULL);
#endif

    const char* errmsg = NULL;
    switch (args_parse_argv(argv, &arguments, &errmsg)) {
        case ARGS_SHOW_HELP: print_help(); exit(EXIT_SUCCESS);
        case ARGS_SHOW_VERSION: printf("%s\n", program_version); exit(EXIT_SUCCESS);
        case ARGS_ERROR:
            fprintf(stderr, "%s: %s\n", app_name, errmsg);
            fprintf(stderr, "Try '" SHORT_NAME " --help' for more information.\n");
            exit(EXIT_FAILURE);
        case ARGS_OK: break;
    }
}
