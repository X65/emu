#include <unistd.h>

#define SOKOL_IMPL
#define SOKOL_GLES3
#define SOKOL_UNREACHABLE __builtin_unreachable()
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_args.h"

#include "icon.c"
#include "cli.h"

extern const char* GIT_TAG;
extern const char* GIT_REV;
extern const char* GIT_BRANCH;
const char* APP_NAME = "X65 emu";

sg_pass_action pass_action;

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    pass_action = (sg_pass_action){
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 1.0f, 0.0f, 0.0f, 1.0f } }
    };

    cli_init();
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
    sargs_shutdown();

    cli_cleanup();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    if (strlen(GIT_TAG))
        printf("%s  %s\n", APP_NAME, GIT_TAG);
    else
        printf("%s  %s@%s\n", APP_NAME, GIT_REV, GIT_BRANCH);

    sargs_setup(&(sargs_desc){
        .argc = argc,
        .argv = argv,
        .buf_size = (int)sysconf(_SC_ARG_MAX),
    });

    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 400,
        .height = 300,
        .window_title = APP_NAME,
        .icon.images = { { .width = app_icon.width,
                           .height = app_icon.height,
                           .pixels = (sapp_range){ &app_icon.pixel_data, app_icon.width * app_icon.height * 4 } } },
        .logger.func = slog_func,
    };
}
