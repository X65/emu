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
    int silent, verbose;
    const char* output_file;
    bool dap;
    const char* dap_port;
} arguments;

void args_parse(int argc, char* argv[]);

extern void app_load_labels(const char* file, bool clear);
