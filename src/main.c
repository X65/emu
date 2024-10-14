#include <err.h>
#include <unistd.h>

#define SOKOL_IMPL
#define SOKOL_GLES3
#define SOKOL_UNREACHABLE __builtin_unreachable()
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "args.h"

#include "icon.c"
#include "cli.h"
#include "cmd.h"

extern const char* GIT_TAG;
extern const char* GIT_REV;
extern const char* GIT_BRANCH;
#define APP_NAME "X65 emu"
const char* app_name = APP_NAME;

sg_pass_action pass_action;

ArgParser* arg_parser = NULL;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    cli_init();
    cmd_init();
}

static void frame(void) {
    float g = pass_action.colors[0].clear_value.g + 0.01f;
    pass_action.colors[0].clear_value.g = (g > 1.0f) ? 0.0f : g;
    sg_begin_pass(&(sg_pass){ .action = pass_action, .swapchain = sglue_swapchain() });
    sg_end_pass();
    sg_commit();

    cli_update();
}

static void cleanup(void) {
    sg_shutdown();

    cli_cleanup();
    cmd_cleanup();

    ap_free(arg_parser);
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
        "\n"
        "\n-h, --help       Show this help."
        "\n-v, --version    Show app version.");

    if (!ap_parse(arg_parser, argc, argv)) {
        errx(EXIT_FAILURE, "cannot parse arguments list");
    }

    // if we reached interactive mode, print app name and version
    printf("%s  %s\n", app_name, version);

    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 400,
        .height = 300,
        .window_title = app_name,
        .icon.images = { { .width = app_icon.width,
                           .height = app_icon.height,
                           .pixels = (sapp_range){ &app_icon.pixel_data, app_icon.width * app_icon.height * 4 } } },
        .logger.func = slog_func,
    };
}
