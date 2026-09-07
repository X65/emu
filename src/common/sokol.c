#define SOKOL_IMPL
#define SOKOL_TRACE_HOOKS
// sokol 3D-API defines are provided by build options
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_time.h"
// Only included here to instantiate the implementation -- nothing in this file calls
// saudio_*. On Emscripten it is left out, because upstream's WebAudio backend drives
// a ScriptProcessorNode from the browser main thread, the same thread as x65_exec()
// and the ImGui debugger; sokol_audio_worklet.c implements the saudio_* API on a real
// audio thread instead.
#if !defined(__EMSCRIPTEN__)
#include "sokol_audio.h"
#endif
#include "sokol_gl.h"
#include "sokol_fetch.h"
#include "sokol_debugtext.h"
#include "sokol_log.h"
#include "sokol_glue.h"
