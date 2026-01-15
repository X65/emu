/*
    Emu - X65 emulator
*/
#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "common.h"
#include "chips/w65c816s.h"
#include "chips/clk.h"
#include "chips/beeper.h"
#undef CHIPS_IMPL
#include "systems/x65.h"
#if defined(CHIPS_USE_UI)
    #define UI_DBG_USE_W65C816S
    #define UI_DASM_USE_W65C816S
    #include "ui.h"
    #include "ui/ui_settings.h"
    #include "ui/ui_chip.h"
    #include "ui/ui_memedit.h"
    #include "ui/ui_dasm.h"
    #include "ui/ui_dbg.h"
    #include "ui/ui_w65c816s.h"
    #include "ui/ui_audio.h"
    #include "ui/ui_display.h"
    #include "ui/ui_snapshot.h"
    #include "ui/ui_x65.h"
#endif

#include <stdlib.h>
#include <unistd.h>

#ifdef USE_UNWIND
    #include <signal.h>
extern void segfault_handler(int sig);
#endif

#include "icon.c"
#include "log.h"
#include "./args.h"
#include "./dap.h"
#include "./hid.h"

extern const char* GIT_TAG;
extern const char* GIT_REV;
extern const char* GIT_BRANCH;
#define APP_NAME "X65 emu"
const char* app_name = APP_NAME;

static const char* settings_key = "Emu.x65";
extern ui_settings_t* settings_load(const char* imgui_ini_key);
int window_width = 0;
int window_height = 0;

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
static void ui_draw_cb(const ui_draw_info_t* draw_info);
static void ui_save_settings_cb(ui_settings_t* settings);
static void ui_boot_cb(x65_t* sys);
static void ui_save_snapshot(size_t slot_index);
static bool ui_load_snapshot(size_t slot_index);
static void ui_load_snapshots_from_storage(void);
static void web_boot(void);
static void web_reset(void);
static bool web_ready(void);
static bool web_load(chips_range_t data);
static void web_input(const char* text);
static void web_dbg_connect(void);
static void web_dbg_disconnect(void);
static void web_dbg_add_breakpoint(uint32_t addr);
static void web_dbg_remove_breakpoint(uint32_t addr);
static void web_dbg_break(void);
static void web_dbg_continue(void);
static void web_dbg_step_next(void);
static void web_dbg_step_into(void);
static void web_dbg_on_stopped(int stop_reason, uint32_t addr);
static void web_dbg_on_continued(void);
static void web_dbg_on_reboot(void);
static void web_dbg_on_reset(void);
static webapi_cpu_state_t web_dbg_cpu_state(void);
static void web_dbg_request_disassemly(uint32_t addr, int offset_lines, int num_lines, webapi_dasm_line_t* result);
static void web_dbg_read_memory(uint32_t addr, int num_bytes, uint8_t* dst_ptr);
static void* labels = NULL;
    #define BORDER_TOP (24)
#else
    #define BORDER_TOP (8)
#endif
#define BORDER_LEFT       (8)
#define BORDER_RIGHT      (8)
#define BORDER_BOTTOM     (32)
#define LOAD_DELAY_FRAMES (6)

// audio-streaming callback
static void push_audio(const float* samples, int num_samples, void* user_data) {
    (void)user_data;
    saudio_push(samples, num_samples / X65_AUDIO_CHANNELS);
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

static void app_load_rom_labels(const char* rom_file) {
#ifndef USE_WEB
    if (rom_file && strlen(rom_file) >= 4) {
        char buf[PATH_MAX];
        strncpy(buf, rom_file, PATH_MAX);
        buf[PATH_MAX - 1] = '\0';
        const size_t path_len = strlen(buf);
        if (buf[path_len - 4] == '.') {
            strcpy(&buf[path_len - 4], ".lbl");

            if (access(buf, R_OK) == 0) {
                LOG_INFO("Loading labels file: %s", buf);
                app_load_labels(buf, true);
            }
        }
    }
#endif
}

void app_init(void) {
    saudio_setup(&(saudio_desc){
        .num_channels = X65_AUDIO_CHANNELS,
        .logger.func = slog_func,
    });
    x65_joystick_type_t joy_type = arguments.joy ? X65_JOYSTICKTYPE_DIGITAL_1 : X65_JOYSTICKTYPE_NONE;
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
        .init_extra_cb = ui_preinit,
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
    hid_init();
#ifdef CHIPS_USE_UI
    ui_init(&(ui_desc_t){
        .draw_cb = ui_draw_cb,
        .save_settings_cb = ui_save_settings_cb,
        .imgui_ini_key = settings_key,
    });
    ui_x65_init(&state.ui, &(ui_x65_desc_t){
        .x65 = &state.x65,
        .boot_cb = ui_boot_cb,
        .inject = {
            .menu_cb = ui_draw_sokol_menu,
        },
        .dbg_texture = {
            .create_cb = ui_create_texture,
            .update_cb = ui_update_texture,
            .destroy_cb = ui_destroy_texture,
        },
        .dbg_debug = {
            .reboot_cb = web_dbg_on_reboot,
            .reset_cb = web_dbg_on_reset,
            .stopped_cb = web_dbg_on_stopped,
            .continued_cb = web_dbg_on_continued,
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
        },
        .labels = labels,
    });
    ui_x65_load_settings(&state.ui, ui_settings());
    ui_load_snapshots_from_storage();
    // important: initialize webapi after ui
    webapi_init(&(webapi_desc_t){
        .funcs = {
            .boot = web_boot,
            .reset = web_reset,
            .ready = web_ready,
            .load = web_load,
            .input = web_input,
            .dbg_connect = web_dbg_connect,
            .dbg_disconnect = web_dbg_disconnect,
            .dbg_add_breakpoint = web_dbg_add_breakpoint,
            .dbg_remove_breakpoint = web_dbg_remove_breakpoint,
            .dbg_break = web_dbg_break,
            .dbg_continue = web_dbg_continue,
            .dbg_step_next = web_dbg_step_next,
            .dbg_step_into = web_dbg_step_into,
            .dbg_cpu_state = web_dbg_cpu_state,
            .dbg_request_disassembly = web_dbg_request_disassemly,
            .dbg_read_memory = web_dbg_read_memory,
        },
    });
    #ifdef USE_DAP
    dap_init(&(dap_desc_t){
        .stdio = arguments.dap,
        .port = arguments.dap_port,
        .funcs = {
            .boot = web_boot,
            .reset = web_reset,
            .ready = web_ready,
            .load = web_load,
            .input = web_input,
            .dbg_connect = web_dbg_connect,
            .dbg_disconnect = web_dbg_disconnect,
            .dbg_add_breakpoint = web_dbg_add_breakpoint,
            .dbg_remove_breakpoint = web_dbg_remove_breakpoint,
            .dbg_break = web_dbg_break,
            .dbg_continue = web_dbg_continue,
            .dbg_step_next = web_dbg_step_next,
            .dbg_step_into = web_dbg_step_into,
            .dbg_cpu_state = web_dbg_cpu_state,
            .dbg_request_disassembly = web_dbg_request_disassemly,
            .dbg_read_memory = web_dbg_read_memory,
        },
        .memory = state.x65.ram,
    });
    #endif
#endif
    bool delay_input = false;
    if (arguments.rom) {
        delay_input = true;
        LOG_INFO("Loading ROM: %s", arguments.rom);
        fs_load_file_async(FS_CHANNEL_IMAGES, arguments.rom);
        app_load_rom_labels(arguments.rom);
    }
    if (sargs_exists("prg")) {
        fs_load_base64(FS_CHANNEL_IMAGES, "url.prg", sargs_value("prg"));
    }
    if (!delay_input) {
        if (sargs_exists("input")) {
            keybuf_put(sargs_value("input"));
        }
    }
    if (sargs_exists("break")) {
        int opcode;
        if (sscanf(sargs_value("break"), "%x", &opcode) == 1) {
            ui_dbg_control_opcode_break(&state.ui.dbg, true, opcode);
        }
        else {
            fprintf(stderr, "Bad breakpoint opcode %s\n", sargs_value("break"));
        }
    }
}

static void handle_file_loading(void);
static void send_keybuf_input(void);
static void draw_status_bar(void);

void app_frame(void) {
    state.frame_time_us = clock_frame_time();
    const uint64_t emu_start_time = stm_now();
    state.ticks = x65_exec(&state.x65, state.frame_time_us);
    state.emu_time_ms = stm_ms(stm_since(emu_start_time));
    draw_status_bar();
    gfx_draw(x65_display_info(&state.x65));
    handle_file_loading();
    send_keybuf_input();
    sdl_poll_events();
#ifdef USE_DAP
    dap_process();
#endif
}

void app_input(const sapp_event* event) {
    window_width = event->window_width;
    window_height = event->window_height;

    // accept dropped files also when ImGui grabs input
    if (event->type == SAPP_EVENTTYPE_FILES_DROPPED) {
        fs_load_dropped_file_async(FS_CHANNEL_IMAGES);
    }
#ifdef CHIPS_USE_UI
    if (ui_input(event)) {
        // input was handled by UI
        return;
    }
#endif
    switch (event->type) {
        case SAPP_EVENTTYPE_KEY_DOWN: x65_key_down(&state.x65, event->key_code); break;
        case SAPP_EVENTTYPE_KEY_UP:
            x65_key_up(&state.x65, event->key_code);
            if (event->key_code == SAPP_KEYCODE_Q) {
                if (event->modifiers == SAPP_MODIFIER_SUPER || event->modifiers == SAPP_MODIFIER_CTRL) {
                    sapp_request_quit();
                }
            }
            break;
        default: break;
    }
}

void app_cleanup(void) {
    x65_discard(&state.x65);
#ifdef CHIPS_USE_UI
    ui_x65_discard(&state.ui);
    ui_discard();
#endif
    saudio_shutdown();
    gfx_shutdown();
    sargs_shutdown();
    hid_shutdown();
#ifdef USE_DAP
    dap_shutdown();
#endif
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
    if (fs_success(FS_CHANNEL_IMAGES) && clock_frame_count_60hz() > load_delay_frames) {
        bool load_success = false;
        if (fs_ext(FS_CHANNEL_IMAGES, "txt") || fs_ext(FS_CHANNEL_IMAGES, "bas")) {
            load_success = true;
            keybuf_put((const char*)fs_data(FS_CHANNEL_IMAGES).ptr);
        }
        else if (fs_ext(FS_CHANNEL_IMAGES, "xex")) {
            load_success = x65_quickload_xex(&state.x65, fs_data(FS_CHANNEL_IMAGES));
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
        fs_reset(FS_CHANNEL_IMAGES);
    }
    else if (fs_failed(FS_CHANNEL_IMAGES)) {
        gfx_flash_error();
        fs_reset(FS_CHANNEL_IMAGES);
    }
}

static void draw_status_bar(void) {
    prof_push(PROF_EMU, (float)state.emu_time_ms);
    prof_stats_t emu_stats = prof_stats(PROF_EMU);
    const float frame_time = (float)state.frame_time_us * 0.001f;

    const uint32_t text_color = 0xFFFFFFFF;
    const uint32_t joy_active = 0xFFFFEE00;
    const uint32_t joy_inactive = 0xFF886600;

    const float w = sapp_widthf();
    const float h = sapp_heightf();
    sdtx_canvas(w, h);
    if (emu_stats.avg_val > frame_time)
        sdtx_color3b(255, 32, 32);
    else
        sdtx_color3b(255, 255, 255);
    sdtx_origin(1.0f, (h / 8.0f) - 3.5f);
    sdtx_font(0);

    // joystick state
    sdtx_puts("JOYSTICK: ");
    const uint8_t joymask = x65_joystick_mask(&state.x65);
    sdtx_font(1);
    switch (state.x65.joystick_type) {
        case X65_JOYSTICKTYPE_DIGITAL_1: sdtx_puts("1 "); break;
        case X65_JOYSTICKTYPE_DIGITAL_2: sdtx_puts("2 "); break;
        case X65_JOYSTICKTYPE_DIGITAL_12: sdtx_puts("12 "); break;
        case X65_JOYSTICKTYPE_NONE: break;
    }
    sdtx_color1i((joymask & X65_JOYSTICK_LEFT) ? joy_active : joy_inactive);
    sdtx_putc(0x88);  // arrow left
    sdtx_color1i((joymask & X65_JOYSTICK_RIGHT) ? joy_active : joy_inactive);
    sdtx_putc(0x89);  // arrow right
    sdtx_color1i((joymask & X65_JOYSTICK_UP) ? joy_active : joy_inactive);
    sdtx_putc(0x8B);  // arrow up
    sdtx_color1i((joymask & X65_JOYSTICK_DOWN) ? joy_active : joy_inactive);
    sdtx_putc(0x8A);  // arrow down
    sdtx_color1i((joymask & X65_JOYSTICK_BTN) ? joy_active : joy_inactive);
    sdtx_putc(0x87);  // btn
    sdtx_color1i((joymask & X65_JOYSTICK_BTN2) ? joy_active : joy_inactive);
    sdtx_putc(0x87);  // btn
    sdtx_color1i((joymask & X65_JOYSTICK_BTN3) ? joy_active : joy_inactive);
    sdtx_putc(0x87);  // btn
    sdtx_color1i((joymask & X65_JOYSTICK_BTN4) ? joy_active : joy_inactive);
    sdtx_putc(0x87);  // btn
    sdtx_font(0);

    // RGB LEDs
    uint32_t* leds;
    size_t leds_no;
    ria816_rgb_get_leds(&leds, &leds_no);
    sdtx_color1i(text_color);
    sdtx_puts("  LEDs: ");
    for (size_t i = 0; i < leds_no; i++) {
        // GRB8 order ¯\_(ツ)_/¯
        uint32_t led = leds[i];
        if (led != 0) {
            sdtx_color3b((led >> 8) & 0xFF, (led >> 16) & 0xFF, led & 0xFF);
            sdtx_putc(0xCF);  // filled circle
        }
        else {
            sdtx_color1i(0xFF444444);
            sdtx_putc(0x8C);  // empty circle
        }
        sdtx_putc(' ');
    }

    sdtx_font(0);
    sdtx_color1i(text_color);
    sdtx_pos(0.0f, 1.5f);
    sdtx_printf(
        "frame:%.2fms emu:%.2fms (min:%.2fms max:%.2fms) ticks:%d",
        frame_time,
        emu_stats.avg_val,
        emu_stats.min_val,
        emu_stats.max_val,
        state.ticks);
}

#if defined(CHIPS_USE_UI)
static void ui_draw_cb(const ui_draw_info_t* draw_info) {
    ui_x65_draw(
        &state.ui,
        &(ui_x65_frame_t){
            .display = draw_info->display,
        });
}

static void ui_save_settings_cb(ui_settings_t* settings) {
    ui_x65_save_settings(&state.ui, settings);
    settings->window_width = window_width;
    settings->window_height = window_height;
}

static void ui_boot_cb(x65_t* sys) {
    clock_init();
    x65_desc_t desc = x65_desc(sys->joystick_type);
    x65_init(sys, &desc);
    if (arguments.rom) {
        LOG_INFO("Loading ROM: %s", arguments.rom);
        fs_load_file_async(FS_CHANNEL_IMAGES, arguments.rom);
        app_load_rom_labels(arguments.rom);
    }
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
        fs_load_snapshot_async("x65", snapshot_slot, ui_fetch_snapshot_callback);
    }
}

// webapi wrappers
static void web_boot(void) {
    clock_init();
    x65_desc_t desc = x65_desc(state.x65.joystick_type);
    x65_init(&state.x65, &desc);
    ui_dbg_reboot(&state.ui.dbg);
}

static void web_reset(void) {
    x65_reset(&state.x65);
    ui_dbg_reset(&state.ui.dbg);
}

static void web_dbg_connect(void) {
    gfx_disable_speaker_icon();
    state.dbg.entry_addr = 0xFFFFFFFF;
    state.dbg.exit_addr = 0xFFFFFFFF;
    ui_dbg_external_debugger_connected(&state.ui.dbg);
}

static void web_dbg_disconnect(void) {
    state.dbg.entry_addr = 0xFFFFFFFF;
    state.dbg.exit_addr = 0xFFFFFFFF;
    ui_dbg_external_debugger_disconnected(&state.ui.dbg);
}

static bool web_ready(void) {
    return clock_frame_count_60hz() > LOAD_DELAY_FRAMES;
}

static bool web_load(chips_range_t data) {
    if (data.size <= sizeof(webapi_fileheader_t)) {
        return false;
    }
    const webapi_fileheader_t* hdr = (webapi_fileheader_t*)data.ptr;
    if ((hdr->type[0] != 'P') || (hdr->type[1] != 'R') || (hdr->type[2] != 'G') || (hdr->type[3] != ' ')) {
        return false;
    }
    const uint16_t start_addr = (hdr->start_addr_hi << 8) | hdr->start_addr_lo;
    const chips_range_t prg = {
        .ptr = (void*)&hdr->payload,
        .size = data.size - sizeof(webapi_fileheader_t),
    };
    bool loaded = x65_quickload_xex(&state.x65, prg);
    if (loaded) {
        state.dbg.entry_addr = start_addr;
        ui_dbg_add_breakpoint(&state.ui.dbg, state.dbg.entry_addr);
        // if debugger is stopped, unstuck it
        if (ui_dbg_stopped(&state.ui.dbg)) {
            ui_dbg_continue(&state.ui.dbg, false);
        }
    }
    return loaded;
}

static void web_input(const char* text) {
    keybuf_put(text);
}

static void web_dbg_add_breakpoint(uint32_t addr) {
    ui_dbg_add_breakpoint(&state.ui.dbg, addr);
}

static void web_dbg_remove_breakpoint(uint32_t addr) {
    ui_dbg_remove_breakpoint(&state.ui.dbg, addr);
}

static void web_dbg_break(void) {
    ui_dbg_break(&state.ui.dbg);
}

static void web_dbg_continue(void) {
    ui_dbg_continue(&state.ui.dbg, false);
}

static void web_dbg_step_next(void) {
    ui_dbg_step_next(&state.ui.dbg);
}

static void web_dbg_step_into(void) {
    ui_dbg_step_into(&state.ui.dbg);
}

static void web_dbg_on_stopped(int stop_reason, uint32_t addr) {
    // stopping on the entry or exit breakpoints always
    // overrides the incoming stop_reason
    int webapi_stop_reason = WEBAPI_STOPREASON_UNKNOWN;
    if (state.dbg.entry_addr == state.x65.cpu.PC) {
        webapi_stop_reason = WEBAPI_STOPREASON_ENTRY;
    }
    else if (state.dbg.exit_addr == state.x65.cpu.PC) {
        webapi_stop_reason = WEBAPI_STOPREASON_EXIT;
    }
    else if (stop_reason == UI_DBG_STOP_REASON_BREAK) {
        webapi_stop_reason = WEBAPI_STOPREASON_BREAK;
    }
    else if (stop_reason == UI_DBG_STOP_REASON_STEP) {
        webapi_stop_reason = WEBAPI_STOPREASON_STEP;
    }
    else if (stop_reason == UI_DBG_STOP_REASON_BREAKPOINT) {
        webapi_stop_reason = WEBAPI_STOPREASON_BREAKPOINT;
    }
    webapi_event_stopped(webapi_stop_reason, addr);
    #ifdef USE_DAP
    dap_event_stopped(webapi_stop_reason, addr);
    #endif
}

static void web_dbg_on_continued(void) {
    webapi_event_continued();
    #ifdef USE_DAP
    dap_event_continued();
    #endif
}

static void web_dbg_on_reboot(void) {
    webapi_event_reboot();
    #ifdef USE_DAP
    dap_event_reboot();
    #endif
}

static void web_dbg_on_reset(void) {
    webapi_event_reset();
    #ifdef USE_DAP
    dap_event_reset();
    #endif
}

static webapi_cpu_state_t web_dbg_cpu_state(void) {
    const w65816_t* cpu = &state.x65.cpu;
    return (webapi_cpu_state_t){
        .items = {
            [WEBAPI_CPUSTATE_TYPE] = WEBAPI_CPUTYPE_65816,
            [WEBAPI_CPUSTATE_6502_A] = cpu->C,
            [WEBAPI_CPUSTATE_6502_X] = cpu->X,
            [WEBAPI_CPUSTATE_6502_Y] = cpu->Y,
            [WEBAPI_CPUSTATE_6502_S] = cpu->S,
            [WEBAPI_CPUSTATE_6502_P] = cpu->P,
            [WEBAPI_CPUSTATE_6502_PC] = cpu->PC,
            [WEBAPI_CPUSTATE_65816_D] = cpu->D,
            [WEBAPI_CPUSTATE_65816_DBR] = cpu->DBR,
            [WEBAPI_CPUSTATE_65816_PBR] = cpu->PBR,
            [WEBAPI_CPUSTATE_65816_E] = cpu->emulation,
        }
    };
}

static void web_dbg_request_disassemly(uint32_t addr, int offset_lines, int num_lines, webapi_dasm_line_t* result) {
    assert(num_lines > 0);
    ui_dbg_dasm_line_t* lines = calloc((size_t)num_lines, sizeof(ui_dbg_dasm_line_t));
    ui_dbg_disassemble(
        &state.ui.dbg,
        &(ui_dbg_dasm_request_t){
            .addr = addr,
            .num_lines = num_lines,
            .offset_lines = offset_lines,
            .out_lines = lines,
        });
    for (int line_idx = 0; line_idx < num_lines; line_idx++) {
        const ui_dbg_dasm_line_t* src = &lines[line_idx];
        webapi_dasm_line_t* dst = &result[line_idx];
        dst->addr = src->addr;
        dst->num_bytes = (src->num_bytes <= WEBAPI_DASM_LINE_MAX_BYTES) ? src->num_bytes : WEBAPI_DASM_LINE_MAX_BYTES;
        dst->num_chars = (src->num_chars <= WEBAPI_DASM_LINE_MAX_CHARS) ? src->num_chars : WEBAPI_DASM_LINE_MAX_CHARS;
        memcpy(dst->bytes, src->bytes, dst->num_bytes);
        memcpy(dst->chars, src->chars, dst->num_chars);
    }
    free(lines);
}

static void web_dbg_read_memory(uint32_t addr, int num_bytes, uint8_t* dst_ptr) {
    for (int i = 0; i < num_bytes; i++) {
        const uint8_t bank = (addr >> 16) & 0xFF;
        *dst_ptr++ = mem_rd(&state.x65, bank, addr++ & 0xFFFF);
    }
}
#endif

void app_load_labels(const char* file, bool clear) {
#ifdef CHIPS_USE_UI
    labels = ui_dasm_load_labels(&state.ui.dasm[0], file, labels, clear);
#endif
}

static unsigned long djb2(const char* s) {
    unsigned long h = 5381;
    int c;
    while ((c = *s++))
        h = ((h << 5) + h) + c;  // h * 33 + c
    return h;
}

void log_func(uint32_t log_level, const char* log_id, const char* filename, uint32_t line_nr, const char* fmt, ...) {
    char message[512];

    char* short_filename = strstr(filename, "src/");
    if (short_filename) {
        short_filename += 4;
    }

    unsigned long log_id_hash = djb2(log_id);
    uint32_t log_item = (log_id_hash >> 32) ^ (log_id_hash & 0xFFFFFFFF);

    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    message[sizeof(message) - 1] = '\0';
    va_end(args);

    slog_func(log_id, log_level, log_item, message, line_nr, short_filename, NULL);
    ui_app_log_add(log_level, log_item, log_id, message);
}

char app_version[256];
char program_version[256];

sapp_desc sokol_main(int argc, char* argv[]) {
#ifdef USE_UNWIND
    signal(SIGSEGV, segfault_handler);
#endif

    if (strlen(GIT_TAG))
        snprintf(app_version, sizeof(app_version), "%s", GIT_TAG);
    else
        snprintf(app_version, sizeof(app_version), "%s@%s", GIT_REV, GIT_BRANCH);

    snprintf(program_version, sizeof(program_version), "emu %s\n%s", app_version, full_name);

    sargs_setup(&(sargs_desc){
        .argc = argc,
        .argv = argv,
#ifdef _WIN32
        .buf_size = (int)_ARGMAX,
#else
        .buf_size = (int)sysconf(_SC_ARG_MAX),
#endif
    });
    args_parse(argc, argv);

    app_load_rom_labels(arguments.rom);

    ui_settings_t* ui_setts = settings_load(settings_key);
    if (ui_setts) {
        window_width = ui_setts->window_width;
        window_height = ui_setts->window_height;
    }

    const chips_display_info_t info = x65_display_info(0);
    const int default_width = info.screen.width + BORDER_LEFT + BORDER_RIGHT;
    const int default_height = info.screen.height + BORDER_TOP + BORDER_BOTTOM;
    return (sapp_desc){
        .init_cb = app_init,
        .frame_cb = app_frame,
        .event_cb = app_input,
        .cleanup_cb = app_cleanup,
        .width = window_width >= default_width ? window_width : default_width,
        .height = window_height >= default_height ? window_height : default_height,
        .window_title = app_name,
        .icon.images = {
            {
                .width = app_icon.width,
                .height = app_icon.height,
                .pixels = (sapp_range){ &app_icon.pixel_data, app_icon.width * app_icon.height * 4 },
            },
        },
        .enable_clipboard = true,
        .enable_dragndrop = true,
        .html5 = {
            .bubble_mouse_events = true,
            .update_document_title = true,
        },
        .logger.func = slog_func,
    };
}
