/*
    Emu - X65 emulator
*/
#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "common.h"
#include "chips/m6502.h"
#include "chips/ria816.h"
#include "chips/cgia.h"
#include "chips/ymf825.h"
#include "chips/mcp23017.h"
#include "chips/kbd.h"
#include "chips/clk.h"

#include "systems/x65.h"
// #include "x65-os816.h"
#if defined(CHIPS_USE_UI)
    #define UI_DBG_USE_M6502
    #include "ui.h"
    #include "ui/ui_chip.h"
    #include "ui/ui_memedit.h"
    #include "ui/ui_memmap.h"
    #include "ui/ui_dasm.h"
    #include "ui/ui_dbg.h"
    #include "ui/ui_m6502.h"
    #include "ui/ui_audio.h"
    #include "ui/ui_kbd.h"
    #include "ui/ui_snapshot.h"
    #include "systems/ui_x65.h"
#endif

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <argp.h>

#include "icon.c"
#include "cli.h"
#include "cmd.h"

extern const char* GIT_TAG;
extern const char* GIT_REV;
extern const char* GIT_BRANCH;
#define APP_NAME "X65 emu"
const char* app_name = APP_NAME;

const char* argp_program_bug_address = " https://github.com/X65/emu/issues ";
static char app_doc[] = "X65 microcomputer emulator";

static char args_doc[] = "[ROM.xex]";
static struct argp_option options[] = {
    { "verbose", 'v', 0, 0, "Produce verbose output" },
    { "quiet", 'q', 0, 0, "Don't produce any output" },
    { "silent", 's', 0, OPTION_ALIAS },
    { "output", 'o', "FILE", 0, "Output to FILE instead of standard output" },
    { 0 }
};

typedef struct {
    uint32_t version;
    x65_t x65;
} x65_snapshot_t;

static struct {
    x65_t x65;
    uint32_t frame_time_us;
    uint32_t ticks;
    double emu_time_ms;
#ifdef CHIPS_USE_UI
    ui_x65_t ui;
    struct {
        uint32_t entry_addr;
        uint32_t exit_addr;
    } dbg;
    x65_snapshot_t snapshots[UI_SNAPSHOT_MAX_SLOTS];
#endif
} state;

#ifdef CHIPS_USE_UI
static void ui_draw_cb(void);
static void ui_boot_cb(x65_t* sys);
static void ui_save_snapshot(size_t slot_index);
static bool ui_load_snapshot(size_t slot_index);
static void ui_load_snapshots_from_storage(void);
    #define BORDER_TOP (24)
#else
    #define BORDER_TOP (8)
#endif
#define BORDER_LEFT       (8)
#define BORDER_RIGHT      (8)
#define BORDER_BOTTOM     (16)
#define LOAD_DELAY_FRAMES (180)

// audio-streaming callback
static void push_audio(const float* samples, int num_samples, void* user_data) {
    (void)user_data;
    saudio_push(samples, num_samples);
}

// get x65_desc_t struct based on joystick type
x65_desc_t x65_desc(x65_joystick_type_t joy_type) {
    return (x65_desc_t) {
        .joystick_type = joy_type,
        .audio = {
            .callback = { .func = push_audio },
            .sample_rate = saudio_sample_rate(),
        },
#if defined(CHIPS_USE_UI)
        .debug = ui_x65_get_debug(&state.ui)
#endif
    };
}

static void app_init(void) {
    saudio_setup(&(saudio_desc){
        .logger.func = slog_func,
    });
    x65_joystick_type_t joy_type = X65_JOYSTICKTYPE_NONE;
    if (sargs_exists("joystick")) {
        if (sargs_equals("joystick", "digital_1")) {
            joy_type = X65_JOYSTICKTYPE_DIGITAL_1;
        }
        else if (sargs_equals("joystick", "digital_2")) {
            joy_type = X65_JOYSTICKTYPE_DIGITAL_2;
        }
        else if (sargs_equals("joystick", "digital_12")) {
            joy_type = X65_JOYSTICKTYPE_DIGITAL_12;
        }
    }
    x65_desc_t desc = x65_desc(joy_type);
    x65_init(&state.x65, &desc);
    gfx_init(&(gfx_desc_t){
        .disable_speaker_icon = sargs_exists("disable-speaker-icon"),
#ifdef CHIPS_USE_UI
        .draw_extra_cb = ui_draw,
#endif
        .border = {
            .left = BORDER_LEFT,
            .right = BORDER_RIGHT,
            .top = BORDER_TOP,
            .bottom = BORDER_BOTTOM,
        },
        .display_info = x65_display_info(&state.x65),
    });
    keybuf_init(&(keybuf_desc_t){ .key_delay_frames = 5 });
    clock_init();
    prof_init();
    fs_init();
#ifdef CHIPS_USE_UI
    ui_init(ui_draw_cb);
    ui_x65_init(&state.ui, &(ui_x65_desc_t){
            .x65 = &state.x65,
            .boot_cb = ui_boot_cb,
            .dbg_texture = {
                .create_cb = ui_create_texture,
                .update_cb = ui_update_texture,
                .destroy_cb = ui_destroy_texture,
            },
            .snapshot = {
                .load_cb = ui_load_snapshot,
                .save_cb = ui_save_snapshot,
                .empty_slot_screenshot = {
                    .texture = ui_shared_empty_snapshot_texture(),
                },
            },
            .dbg_keys = {
                .cont = { .keycode = simgui_map_keycode(SAPP_KEYCODE_F5), .name = "F5" },
                .stop = { .keycode = simgui_map_keycode(SAPP_KEYCODE_F5), .name = "F5" },
                .step_over = { .keycode = simgui_map_keycode(SAPP_KEYCODE_F6), .name = "F6" },
                .step_into = { .keycode = simgui_map_keycode(SAPP_KEYCODE_F7), .name = "F7" },
                .step_tick = { .keycode = simgui_map_keycode(SAPP_KEYCODE_F8), .name = "F8" },
                .toggle_breakpoint = { .keycode = simgui_map_keycode(SAPP_KEYCODE_F9), .name = "F9" }
            }
        });
    ui_load_snapshots_from_storage();
#endif
    cli_init();
    cmd_init();

    bool delay_input = false;
    if (sargs_exists("file")) {
        delay_input = true;
        fs_start_load_file(FS_SLOT_IMAGE, sargs_value("file"));
    }
    if (sargs_exists("prg")) {
        fs_load_base64(FS_SLOT_IMAGE, "url.prg", sargs_value("prg"));
    }
    if (!delay_input) {
        if (sargs_exists("input")) {
            keybuf_put(sargs_value("input"));
        }
    }
}

static void handle_file_loading(void);
static void send_keybuf_input(void);
static void draw_status_bar(void);

static void app_frame(void) {
    state.frame_time_us = clock_frame_time();
    const uint64_t emu_start_time = stm_now();
    state.ticks = x65_exec(&state.x65, state.frame_time_us);
    state.emu_time_ms = stm_ms(stm_since(emu_start_time));
    draw_status_bar();
    gfx_draw(x65_display_info(&state.x65));
    handle_file_loading();
    send_keybuf_input();
    cli_update();
}

void app_input(const sapp_event* event) {
    // accept dropped files also when ImGui grabs input
    if (event->type == SAPP_EVENTTYPE_FILES_DROPPED) {
        fs_start_load_dropped_file(FS_SLOT_IMAGE);
    }
#ifdef CHIPS_USE_UI
    if (ui_input(event)) {
        // input was handled by UI
        return;
    }
#endif
    const bool shift = event->modifiers & SAPP_MODIFIER_SHIFT;
    switch (event->type) {
        int c;
        case SAPP_EVENTTYPE_CHAR:
            c = (int)event->char_code;
            if ((c > 0x20) && (c < 0x7F)) {
                // need to invert case (unshifted is upper caps, shifted is lower caps
                if (isupper(c)) {
                    c = tolower(c);
                }
                else if (islower(c)) {
                    c = toupper(c);
                }
                x65_key_down(&state.x65, c);
                x65_key_up(&state.x65, c);
            }
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:
        case SAPP_EVENTTYPE_KEY_UP:
            switch (event->key_code) {
                case SAPP_KEYCODE_SPACE: c = 0x20; break;
                case SAPP_KEYCODE_LEFT: c = 0x08; break;
                case SAPP_KEYCODE_RIGHT: c = 0x09; break;
                case SAPP_KEYCODE_DOWN: c = 0x0A; break;
                case SAPP_KEYCODE_UP: c = 0x0B; break;
                case SAPP_KEYCODE_ENTER: c = 0x0D; break;
                case SAPP_KEYCODE_BACKSPACE: c = shift ? 0x0C : 0x01; break;
                case SAPP_KEYCODE_ESCAPE: c = shift ? 0x13 : 0x03; break;
                case SAPP_KEYCODE_F1: c = 0xF1; break;
                case SAPP_KEYCODE_F2: c = 0xF2; break;
                case SAPP_KEYCODE_F3: c = 0xF3; break;
                case SAPP_KEYCODE_F4: c = 0xF4; break;
                case SAPP_KEYCODE_F5: c = 0xF5; break;
                case SAPP_KEYCODE_F6: c = 0xF6; break;
                case SAPP_KEYCODE_F7: c = 0xF7; break;
                case SAPP_KEYCODE_F8: c = 0xF8; break;
                default: c = 0; break;
            }
            if (c) {
                if (event->type == SAPP_EVENTTYPE_KEY_DOWN) {
                    x65_key_down(&state.x65, c);
                }
                else {
                    x65_key_up(&state.x65, c);
                }
            }
            break;
        default: break;
    }
}

static void app_cleanup(void) {
    x65_discard(&state.x65);
#ifdef CHIPS_USE_UI
    ui_x65_discard(&state.ui);
    ui_discard();
#endif
    saudio_shutdown();
    gfx_shutdown();
    sargs_shutdown();

    cli_cleanup();
    cmd_cleanup();

    // ap_free(arg_parser);
}

static void send_keybuf_input(void) {
    uint8_t key_code;
    if (0 != (key_code = keybuf_get(state.frame_time_us))) {
        /* FIXME: this is ugly */
        x65_joystick_type_t joy_type = state.x65.joystick_type;
        state.x65.joystick_type = X65_JOYSTICKTYPE_NONE;
        x65_key_down(&state.x65, key_code);
        x65_key_up(&state.x65, key_code);
        state.x65.joystick_type = joy_type;
    }
}

static void handle_file_loading(void) {
    fs_dowork();
    const uint32_t load_delay_frames = LOAD_DELAY_FRAMES;
    if (fs_success(FS_SLOT_IMAGE) && clock_frame_count_60hz() > load_delay_frames) {
        bool load_success = false;
        if (fs_ext(FS_SLOT_IMAGE, "txt") || fs_ext(FS_SLOT_IMAGE, "bas")) {
            load_success = true;
            keybuf_put((const char*)fs_data(FS_SLOT_IMAGE).ptr);
        }
        if (load_success) {
            if (clock_frame_count_60hz() > (load_delay_frames + 10)) {
                gfx_flash_success();
            }
            if (!sargs_exists("debug")) {
                if (sargs_exists("input")) {
                    keybuf_put(sargs_value("input"));
                }
            }
        }
        else {
            gfx_flash_error();
        }
        fs_reset(FS_SLOT_IMAGE);
    }
}

static void draw_status_bar(void) {
    prof_push(PROF_EMU, (float)state.emu_time_ms);
    prof_stats_t emu_stats = prof_stats(PROF_EMU);
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    sdtx_canvas(w, h);
    sdtx_color3b(255, 255, 255);
    sdtx_pos(1.0f, (h / 8.0f) - 1.5f);
    sdtx_printf(
        "frame:%.2fms emu:%.2fms (min:%.2fms max:%.2fms) ticks:%d",
        (float)state.frame_time_us * 0.001f,
        emu_stats.avg_val,
        emu_stats.min_val,
        emu_stats.max_val,
        state.ticks);
}

#if defined(CHIPS_USE_UI)
static void ui_draw_cb(void) {
    ui_x65_draw(&state.ui);
}

static void ui_boot_cb(x65_t* sys) {
    clock_init();
    x65_desc_t desc = x65_desc(sys->joystick_type);
    x65_init(sys, &desc);
}

static void ui_update_snapshot_screenshot(size_t slot) {
    ui_snapshot_screenshot_t screenshot = { .texture = ui_create_screenshot_texture(
                                                x65_display_info(&state.snapshots[slot].x65)) };
    ui_snapshot_screenshot_t prev_screenshot = ui_snapshot_set_screenshot(&state.ui.snapshot, slot, screenshot);
    if (prev_screenshot.texture) {
        ui_destroy_texture(prev_screenshot.texture);
    }
}

static void ui_save_snapshot(size_t slot) {
    if (slot < UI_SNAPSHOT_MAX_SLOTS) {
        state.snapshots[slot].version = x65_save_snapshot(&state.x65, &state.snapshots[slot].x65);
        ui_update_snapshot_screenshot(slot);
        fs_save_snapshot("x65", slot, (chips_range_t){ .ptr = &state.snapshots[slot], sizeof(x65_snapshot_t) });
    }
}

static bool ui_load_snapshot(size_t slot) {
    bool success = false;
    if ((slot < UI_SNAPSHOT_MAX_SLOTS) && (state.ui.snapshot.slots[slot].valid)) {
        success = x65_load_snapshot(&state.x65, state.snapshots[slot].version, &state.snapshots[slot].x65);
    }
    return success;
}

static void ui_fetch_snapshot_callback(const fs_snapshot_response_t* response) {
    assert(response);
    if (response->result != FS_RESULT_SUCCESS) {
        return;
    }
    if (response->data.size != sizeof(x65_snapshot_t)) {
        return;
    }
    if (((x65_snapshot_t*)response->data.ptr)->version != X65_SNAPSHOT_VERSION) {
        return;
    }
    size_t snapshot_slot = response->snapshot_index;
    assert(snapshot_slot < UI_SNAPSHOT_MAX_SLOTS);
    memcpy(&state.snapshots[snapshot_slot], response->data.ptr, response->data.size);
    ui_update_snapshot_screenshot(snapshot_slot);
}

static void ui_load_snapshots_from_storage(void) {
    for (size_t snapshot_slot = 0; snapshot_slot < UI_SNAPSHOT_MAX_SLOTS; snapshot_slot++) {
        fs_start_load_snapshot(FS_SLOT_SNAPSHOTS, "x65", snapshot_slot, ui_fetch_snapshot_callback);
    }
}
#endif

static struct arguments {
    char* rom;
    int silent, verbose;
    char* output_file;
} arguments = { NULL, 0, 0, "-" };

void dump_args(void) {
    printf(
        "ROM = %s\nOUTPUT_FILE = %s\n"
        "VERBOSE = %s\nSILENT = %s\n",
        arguments.rom,
        arguments.output_file,
        arguments.verbose ? "yes" : "no",
        arguments.silent ? "yes" : "no");
}

static error_t parse_opt(int key, char* arg, struct argp_state* argp_state) {
    struct arguments* args = argp_state->input;

    switch (key) {
        case 'q':
        case 's': args->silent = 1; break;
        case 'v': args->verbose = 1; break;
        case 'o': args->output_file = arg; break;

        case ARGP_KEY_ARG:
            if (argp_state->arg_num >= 2) /* Too many arguments. */
                argp_usage(argp_state);

            args->rom = arg;
            break;

        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, app_doc };
static char app_version[256];
static char program_version[256];

sapp_desc sokol_main(int argc, char* argv[]) {
    if (strlen(GIT_TAG))
        snprintf(app_version, sizeof(app_version), "%s", GIT_TAG);
    else
        snprintf(app_version, sizeof(app_version), "%s@%s", GIT_REV, GIT_BRANCH);

    snprintf(program_version, sizeof(program_version), "emu %s\n%s", app_version, app_doc);
    argp_program_version = program_version;

    sargs_setup(&(sargs_desc){
        .argc = argc,
        .argv = argv,
        .buf_size = (int)sysconf(_SC_ARG_MAX),
    });

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    // if we reached interactive mode, print app name and version
    printf("%s  %s\n", app_name, app_version);
    // and reset argp version string, so it does not add --version to command options
    argp_program_version = NULL;

    const chips_display_info_t info = x65_display_info(0);
    return (sapp_desc){
        .init_cb = app_init,
        .frame_cb = app_frame,
        .event_cb = app_input,
        .cleanup_cb = app_cleanup,
        .width = 2 * info.screen.width + BORDER_LEFT + BORDER_RIGHT,
        .height = 2 * info.screen.height + BORDER_TOP + BORDER_BOTTOM,
        .window_title = app_name,
        .icon.images = { { .width = app_icon.width,
                           .height = app_icon.height,
                           .pixels = (sapp_range){ &app_icon.pixel_data, app_icon.width * app_icon.height * 4 } } },
        .enable_dragndrop = true,
        .html5_bubble_mouse_events = true,
        .logger.func = slog_func,
    };
}
