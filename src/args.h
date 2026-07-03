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
} arguments;

void args_parse(int argc, char* argv[]);

extern void app_load_labels(const char* file, bool clear);
