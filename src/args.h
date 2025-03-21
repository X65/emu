#pragma once

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
} arguments;

void args_parse(int argc, char* argv[]);
