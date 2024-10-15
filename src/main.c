#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "common.h"
#include "chips/m6502.h"
#include "chips/clk.h"
#include "systems/x65.h"
#if defined(CHIPS_USE_UI)
    #include "ui.h"
    #include "systems/ui_x65.h"
#endif
#include "sokol_glue.h"
#include "args.h"

#include "icon.c"
#include "cli.h"
#include "cmd.h"

extern const char* GIT_TAG;
extern const char* GIT_REV;
extern const char* GIT_BRANCH;
#define APP_NAME "X65 emu"
const char* app_name = APP_NAME;

ArgParser* arg_parser = NULL;

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

// audio-streaming callback
static void push_audio(const float* samples, int num_samples, void* user_data) {
    (void)user_data;
    saudio_push(samples, num_samples);
}

// get x65_desc_t struct
x65_desc_t x65_desc(void) {
    return (x65_desc_t) {
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
    x65_desc_t desc = x65_desc();
    x65_init(&state.x65, &desc);
    gfx_init(&(gfx_desc_t){
        .disable_speaker_icon = sargs_exists("disable-speaker-icon"),
#ifdef CHIPS_USE_UI
        .draw_extra_cb = ui_draw,
#endif
        .display_info = x65_display_info(&state.x65),
    });
    keybuf_init(&(keybuf_desc_t){ .key_delay_frames = 5 });
    clock_init();
    prof_init();
    fs_init();
#ifdef CHIPS_USE_UI
#endif
    cli_init();
    cmd_init();
}

static void draw_status_bar(void);

static void app_frame(void) {
    state.frame_time_us = clock_frame_time();
    const uint64_t emu_start_time = stm_now();
    state.ticks = x65_exec(&state.x65, state.frame_time_us);
    state.emu_time_ms = stm_ms(stm_since(emu_start_time));
    draw_status_bar();
    gfx_draw(x65_display_info(&state.x65));
    cli_update();
}

static void app_cleanup(void) {
    x65_discard(&state.x65);
#ifdef CHIPS_USE_UI
    ui_x65_discard(&state.ui);
    ui_discard();
#endif
    saudio_shutdown();
    gfx_shutdown();

    cli_cleanup();
    cmd_cleanup();

    ap_free(arg_parser);
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

sapp_desc sokol_main(int argc, char* argv[]) {
    char version[256];
    if (strlen(GIT_TAG))
        snprintf(version, sizeof(version), "%s", GIT_TAG);
    else
        snprintf(version, sizeof(version), "%s@%s", GIT_REV, GIT_BRANCH);

    arg_parser = ap_new_parser();
    if (!arg_parser) {
        errx(EXIT_FAILURE, "cannot create args parser");
    }

    ap_set_version(arg_parser, version);
    ap_set_helptext(
        arg_parser,
        APP_NAME
        "\nUsage: emu [-hv] [ROM file]"
        "\n"
        "\n-h, --help       Show this help."
        "\n-v, --version    Show app version.");

    if (!ap_parse(arg_parser, argc, argv)) {
        errx(EXIT_FAILURE, "cannot parse arguments list");
    }

    // if we reached interactive mode, print app name and version
    printf("%s  %s\n", app_name, version);

    sargs_setup(&(sargs_desc){
        .argc = argc,
        .argv = argv,
        .buf_size = (int)sysconf(_SC_ARG_MAX),
    });

    const chips_display_info_t info = x65_display_info(0);
    return (sapp_desc){
        .init_cb = app_init,
        .frame_cb = app_frame,
        .cleanup_cb = app_cleanup,
        .width = info.screen.width,
        .height = info.screen.height,
        .window_title = app_name,
        .icon.images = { { .width = app_icon.width,
                           .height = app_icon.height,
                           .pixels = (sapp_range){ &app_icon.pixel_data, app_icon.width * app_icon.height * 4 } } },
        .logger.func = slog_func,
    };
}
