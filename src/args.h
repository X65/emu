#pragma once

#include <stdbool.h>

extern const char* app_name;
extern const char* app_releases_address;
extern const char* app_bug_address;
extern const char full_name[];
extern char app_version[];
extern char program_version[];

extern struct arguments {
    const char* rom;
    const char* output_file;
    bool silent, verbose, zeromem, dap, crt, fullscreen, disable_gui, disable_speaker_icon;
    const char* dap_port;
    const char* crt_values;
    const char* break_opcode;
    const char* joystick;
    const char* sgu_dump;
} arguments;

void args_parse(int argc, char* argv[]);

// Outcome of parsing that the caller must act on. Normal parsing fills the
// arguments struct and returns ARGS_OK; --help/--version and errors are
// reported to the caller instead of exiting, so the logic stays testable.
typedef enum {
    ARGS_OK,
    ARGS_SHOW_HELP,
    ARGS_SHOW_VERSION,
    ARGS_ERROR,
} args_status_t;

// Default-initialised arguments (matches the process-wide `arguments` global).
struct arguments args_defaults(void);

// Parse a NULL-terminated argv (argv[0] is the program name) into `out`.
// On ARGS_ERROR, *errmsg (when non-NULL) receives optparse's message.
// Does not print or exit; that is the wrapper's job (see args_parse).
args_status_t args_parse_argv(char** argv, struct arguments* out, const char** errmsg);

// Turn a URL query string (e.g. "?file=rom.xex&crt=1,2,3&fullscreen") into a
// NULL-terminated argv with `prog` as argv[0], suitable for args_parse_argv.
// "file=VALUE" becomes a bare positional; any other token gains a "--" prefix
// unless it already has one. Used by the web build and exercised by tests.
char** args_build_argv_from_query(const char* prog, const char* query);

extern void app_load_labels(const char* file, bool clear);
