#include <err.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "args.h"

#define MAX_ARGS 10

extern ArgParser* arg_parser;
static ArgParser* cmd_parser = NULL;

static ArgParser* cmd_dump_parser = NULL;

__attribute__((__format__(__printf__, 1, 2))) static void cmd_error(const char* format_string, ...) {
    fprintf(stderr, "error: ");

    va_list args;
    va_start(args, format_string);
    vfprintf(stderr, format_string, args);
    va_end(args);

    fprintf(stderr, "\n");
}

static int cmd_dump_callback(char* cmd_name, ArgParser* parser) {
    if (ap_has_args(parser)) {
        cmd_error("the '%s' command does not take arguments", cmd_name);
        return 1;
    }

    puts("--- 8< -- ARGS ------------------ >8 ---");
    ap_print(arg_parser);
    puts("--- 8< -------------------------- >8 ---");
    return 0;
}

static void cmd_free_parser(void) {
    if (cmd_parser) ap_free(cmd_parser);
    cmd_parser = NULL;
}

static void cmd_create_parser(void) {
    cmd_free_parser();

    if (!(cmd_parser = ap_new_parser())) exit(EXIT_FAILURE);
    ap_set_failing(cmd_parser, false);

    if (!(cmd_dump_parser = ap_new_cmd(cmd_parser, "dump"))) exit(EXIT_FAILURE);
    ap_set_helptext(cmd_dump_parser, "Dump emulator state to stdout.");
    ap_set_cmd_callback(cmd_dump_parser, cmd_dump_callback);
}

void cmd_init(void) {}

void cmd_cleanup(void) {
    cmd_free_parser();
}

void cmd_parse_line(const char* line) {
    char* args[MAX_ARGS];            // Array to store pointers to the arguments
    char* line_copy = strdup(line);  // Create a modifiable copy of the input string
    char* token;
    args[0] = "emu";  // Dummy argv[0] for args parser
    int argc = 1;

    // Tokenize the line into command and arguments
    token = strtok(line_copy, " ");
    while (token != NULL && argc < MAX_ARGS) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }

    // Create new parser
    cmd_create_parser();
    // Parse command line
    if (!ap_parse(cmd_parser, argc, args)) {
        errx(EXIT_FAILURE, "cannot parse command line");
    }

    if (strcmp(args[1], "help") && !ap_get_cmd_name(cmd_parser)) {
        cmd_error("'%s' is not a recognised command", args[1]);
    }

    free(line_copy);  // Free the duplicated string
}
