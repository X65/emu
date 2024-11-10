#include <err.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <argp.h>

#include "cmd.h"

extern void dump_args(void);

__attribute__((__format__(__printf__, 1, 2))) static void cmd_error(const char* format_string, ...) {
    fprintf(stderr, "error: ");

    va_list args;
    va_start(args, format_string);
    vfprintf(stderr, format_string, args);
    va_end(args);

    fprintf(stderr, "\n");
}

static struct argp no_argp = { 0 };

#define CMD_NO_ARGS \
    if (cmd_args->arg_num > 0) { \
        cmd_error("the '%s' command does not take arguments", cmd_name); \
        argp_help(&no_argp, stderr, ARGP_HELP_SEE, cmd_name); \
        return; \
    }

static void cmd_no_such(const char* cmd_name) {
    cmd_error("'%s' is not a recognised command", cmd_name);
}

static void cmd_dump_callback(const char* cmd_name, const struct cmd_arguments* cmd_args) {
    CMD_NO_ARGS

    puts("--- 8< -- ARGS ------------------ >8 ---");
    dump_args();
    puts("--- 8< -------------------------- >8 ---");
}

static void cmd_cmds_callback(const char* cmd_name, const struct cmd_arguments* cmd_args);

static struct command_t {
    const char* name;
    const char* help;
    struct argp_option* options;
    const char* args_doc;
    cmd_callback_t callback;
} COMMANDS[] = {
    { "?",    "List commands.",                 0, 0, cmd_cmds_callback },
    { "dump", "Dump emulator state to stdout.", 0, 0, cmd_dump_callback },
};
static const size_t COMMANDS_COUNT = sizeof COMMANDS / sizeof *COMMANDS;

static void print_commands(void) {
    puts("Commands:");
    for (size_t i = 0; i < COMMANDS_COUNT; ++i) {
        printf("%12s - %s\n", COMMANDS[i].name, COMMANDS[i].help);
    }
}

static void cmd_cmds_callback(const char* cmd_name, const struct cmd_arguments* cmd_args) {
    CMD_NO_ARGS

    print_commands();
}

static void print_help(struct command_t* cmd) {
    if (cmd) {
        struct argp argp = { cmd->options, 0, cmd->args_doc, cmd->help, 0, 0 };
        argp_help(&argp, stdout, ARGP_HELP_STD_HELP, cmd->name);
    }
    else
        print_commands();
}

void cmd_init(void) {}

void cmd_cleanup(void) {}

static struct command_t* find_command(const char* cmd_name) {
    for (size_t i = 0; i < COMMANDS_COUNT; ++i) {
        if (0 == strcmp(COMMANDS[i].name, cmd_name)) {
            return &COMMANDS[i];
        }
    }
    return NULL;
}

static error_t parse_opt(int key, char* arg, struct argp_state* state) {
    struct cmd_arguments* arguments = state->input;

    arguments->args[arguments->argc].key = key;
    arguments->args[arguments->argc].arg = arg;
    ++arguments->argc;

    switch (key) {
        case ARGP_KEY_ARG: ++arguments->arg_num; break;
        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static bool did_print_help = false;

char* help_filter(int key, const char* text, void* input) {
    (void)key;
    (void)input;
    did_print_help = true;
    return text;
}

void cmd_parse_line(const char* line) {
    char* args[MAX_ARGS];            // Array to store pointers to the arguments
    char* line_copy = strdup(line);  // Create a modifiable copy of the input string
    char* token;
    unsigned argc = 0;

    // Tokenize the line into command and arguments
    token = strtok(line_copy, " ");
    while (token != NULL && argc < MAX_ARGS) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc > 0) {
        if (0 == strcmp(args[0], "help")) {
            if (argc > 1) {
                for (size_t i = 1; i < argc; ++i) {
                    struct command_t* cmd = find_command(args[i]);
                    if (!cmd) {
                        cmd_no_such(args[i]);
                    }
                    else {
                        print_help(cmd);
                    }
                }
            }
            else
                print_help(NULL);
        }
        else {
            struct command_t* cmd = find_command(args[0]);

            if (!cmd) {
                cmd_no_such(args[0]);
            }
            else {
                struct argp argp = { cmd->options, parse_opt, cmd->args_doc, cmd->help, 0, help_filter };
                struct cmd_arguments arguments = { 0 };
                did_print_help = false;
                error_t err = argp_parse(&argp, argc, args, ARGP_NO_EXIT, 0, &arguments);
                if (!err && !did_print_help) cmd->callback(args[0], &arguments);
            }
        }
    }

    free(line_copy);  // Free the duplicated string
}
