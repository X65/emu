/* Copyright (c) 2026 Tomasz Sterna (smokku)
 *
 * Emscripten replacement implementation for sokol_audio's public API. Compiled only
 * on Emscripten; src/common/sokol.c leaves upstream's implementation out there.
 *
 * Upstream sokol_audio still uses ScriptProcessorNode on Emscripten, which runs the
 * stream callback on the browser main thread -- the same thread that runs x65_exec(),
 * uploads the CGIA framebuffer and draws the whole ImGui debugger, so every hitch
 * there is an audible dropout. Emscripten's Wasm AudioWorklet API gives the callback
 * the same essential execution model as native sokol_audio: it runs on the browser's
 * dedicated real-time audio thread.
 *
 * This implementation is intentionally push-mode only. Emu feeds audio through
 * saudio_push() from push_audio() in src/x65.c, called from inside x65_exec(); it
 * never installs a stream callback. An AudioWorklet is a real second Wasm thread, so
 * sokol_audio's own FIFO cannot be reused here -- its Emscripten mutex is a no-op,
 * which was fine only while producer and consumer were the same thread. The queue
 * below is the real-time-safe SPSC replacement.
 */

#include "sokol_audio.h"

#include <emscripten/html5.h>
#include <emscripten/webaudio.h>

#include <stdalign.h>
#include <stdint.h>
#include <string.h>

#define SAUDIO_WORKLET_NAME       "sokol-audio-worklet"
#define SAUDIO_WORKLET_STACK_SIZE (64 * 1024)

/* Queue capacity in frames. Matches upstream's default FIFO (num_packets *
   packet_frames == 64 * 128), and a power of two so the wrap is a mask. */
#define SAUDIO_FIFO_FRAMES  (8192u)
#define SAUDIO_MAX_CHANNELS (2)

/* How much has to be queued before the worklet starts consuming, and again after
   every underrun. Upstream got this buffering for free: its ScriptProcessorNode
   pulled 2048 frames at a time and _saudio_fifo_read() returned silence unless a
   whole buffer was ready. An AudioWorklet quantum is only 128 frames, so without an
   explicit gate the queue would sit near empty and crackle on every burst. */
#define SAUDIO_DEFAULT_PREFILL_FRAMES (2048u)

typedef struct {
    /* setup_called latches for the lifetime of a failed setup too, so a backend that
       could not finish coming up stays down instead of being retried per frame;
       valid additionally means the AudioContext exists and so has to be reclaimed. */
    bool setup_called;
    bool valid;
    int active;
    int sample_rate;
    int num_channels;
    int buffer_frames;  // the AudioWorklet render quantum
    uint32_t prefill;   // frames the queue must hold before playback starts
    saudio_desc desc;
    EMSCRIPTEN_WEBAUDIO_T context;
    EMSCRIPTEN_WEBAUDIO_T node;
} saudio_worklet_state;

static saudio_worklet_state s_audio;
static alignas(16) uint8_t s_worklet_stack[SAUDIO_WORKLET_STACK_SIZE];

/* The queue. Static storage, never freed: emscripten_destroy_web_audio_node() does
   not join an in-flight process() call, so handing its backing memory back to the
   allocator during shutdown would be a use-after-free.

   s_write_pos and s_read_pos are monotonically increasing frame counters, each with
   exactly one writer. (w - r) stays correct across the uint32_t wrap because the
   capacity is a power of two that divides 2^32, which also means full and empty are
   distinguishable without sacrificing a slot. */
static float s_fifo[SAUDIO_FIFO_FRAMES * SAUDIO_MAX_CHANNELS];
static uint32_t s_write_pos;  // producer (main thread) owns
static uint32_t s_read_pos;   // consumer (worklet thread) owns
static uint32_t s_priming;    // consumer owns: output silence until prefilled

static void saudio_log(uint32_t level, const char* message) {
    if (s_audio.desc.logger.func)
        s_audio.desc.logger.func(
            "saudio",
            level,
            SAUDIO_LOGITEM_OK,
            message,
            0,
            "sokol_audio_worklet.c",
            s_audio.desc.logger.user_data);
}

// zero every channel of `out` from frame `from` onwards
static void saudio_silence(AudioSampleFrame* out, uint32_t from) {
    const size_t frames = (size_t)out->samplesPerChannel;
    // WebAudio lays an output's channels out contiguously, so a whole-buffer wipe is
    // one memset; only the underrun tail needs the per-channel walk
    if (from == 0) {
        memset(out->data, 0, frames * (size_t)out->numberOfChannels * sizeof(float));
        return;
    }
    for (int c = 0; c < out->numberOfChannels; c++) {
        memset(&out->data[(size_t)c * frames + from], 0, (frames - from) * sizeof(float));
    }
}

/* Called from the audio thread. Allocates nothing, takes no lock and makes no JS
   call, so it cannot block the render quantum -- note that emmalloc guards its heap
   with a spinlock under shared memory, which is exactly what must not be touched
   here. */
static bool saudio_process(
    int num_inputs,
    const AudioSampleFrame* inputs,
    int num_outputs,
    AudioSampleFrame* outputs,
    int num_params,
    const AudioParamFrame* params,
    void* user_data) {
    (void)num_inputs;
    (void)inputs;
    (void)num_params;
    (void)params;
    (void)user_data;

    if (num_outputs < 1) return true;

    AudioSampleFrame* out = &outputs[0];
    const int frames = out->samplesPerChannel;
    const int channels = out->numberOfChannels;

    if (!__atomic_load_n(&s_audio.active, __ATOMIC_ACQUIRE) || channels != s_audio.num_channels) {
        saudio_silence(out, 0);
        return true;
    }

    const uint32_t r = __atomic_load_n(&s_read_pos, __ATOMIC_RELAXED);   // we own it
    const uint32_t w = __atomic_load_n(&s_write_pos, __ATOMIC_ACQUIRE);  // pairs with the push release
    const uint32_t avail = w - r;

    if (s_priming && avail < s_audio.prefill) {
        saudio_silence(out, 0);
        return true;
    }
    s_priming = 0;

    uint32_t n = avail;
    if (n > (uint32_t)frames) n = (uint32_t)frames;

    // sokol hands out interleaved frames, WebAudio wants planar channels
    for (uint32_t i = 0; i < n; i++) {
        const float* src = &s_fifo[(size_t)((r + i) & (SAUDIO_FIFO_FRAMES - 1)) * (size_t)channels];
        for (int c = 0; c < channels; c++) {
            out->data[(size_t)c * (size_t)frames + i] = src[c];
        }
    }
    if (n < (uint32_t)frames) {
        // underrun: pad with silence and wait for the queue to refill, so a starved
        // producer costs one clean gap rather than continuous crackle
        saudio_silence(out, n);
        s_priming = 1;
    }

    __atomic_store_n(&s_read_pos, r + n, __ATOMIC_RELEASE);  // releases the slots
    return true;
}

static void saudio_processor_created(EMSCRIPTEN_WEBAUDIO_T context, bool success, void* user_data) {
    (void)user_data;
    if (!s_audio.setup_called) return;  // shutdown raced us
    if (!success) {
        saudio_log(1, "AudioWorklet processor creation failed");
        return;
    }

    int output_channels[1] = { s_audio.num_channels };
    EmscriptenAudioWorkletNodeCreateOptions options = {
        .numberOfOutputs = 1,
        .outputChannelCounts = output_channels,
        .channelCount = (unsigned long)s_audio.num_channels,
        .channelCountMode = WEBAUDIO_CHANNEL_COUNT_MODE_EXPLICIT,
        .channelInterpretation = WEBAUDIO_CHANNEL_INTERPRETATION_SPEAKERS,
    };

    s_audio.node =
        emscripten_create_wasm_audio_worklet_node(context, SAUDIO_WORKLET_NAME, &options, saudio_process, NULL);
    if (!s_audio.node) {
        saudio_log(1, "AudioWorklet node creation failed");
        return;
    }

    emscripten_audio_node_connect(s_audio.node, context, 0, 0);
    __atomic_store_n(&s_audio.active, 1, __ATOMIC_RELEASE);
}

static void saudio_worklet_started(EMSCRIPTEN_WEBAUDIO_T context, bool success, void* user_data) {
    (void)user_data;
    if (!s_audio.setup_called) return;  // shutdown raced us
    if (!success) {
        saudio_log(1, "AudioWorklet thread initialization failed");
        return;
    }

    WebAudioWorkletProcessorCreateOptions options = { .name = SAUDIO_WORKLET_NAME };
    emscripten_create_wasm_audio_worklet_processor_async(context, &options, saudio_processor_created, NULL);
}

static void saudio_resume(void) {
    if (s_audio.context && emscripten_audio_context_state(s_audio.context) != AUDIO_CONTEXT_STATE_RUNNING)
        emscripten_resume_audio_context_sync(s_audio.context);
}

/* WebAudio autoplay policy: resume from the first user gesture. One thunk per event
   struct, since html5.h types the callback by event. These stay registered rather
   than firing once, so a context the browser suspends again later still recovers. */
#define SAUDIO_RESUME_CB(name, event_type)                                 \
    static bool name(int type, const event_type* event, void* user_data) { \
        (void)type;                                                        \
        (void)event;                                                       \
        (void)user_data;                                                   \
        saudio_resume();                                                   \
        return false;                                                      \
    }
SAUDIO_RESUME_CB(saudio_mouse_resume, EmscriptenMouseEvent)
SAUDIO_RESUME_CB(saudio_touch_resume, EmscriptenTouchEvent)
SAUDIO_RESUME_CB(saudio_key_resume, EmscriptenKeyboardEvent)

void saudio_setup(const saudio_desc* desc) {
    if (!desc || s_audio.setup_called) return;

    memset(&s_audio, 0, sizeof(s_audio));
    s_audio.setup_called = true;
    s_audio.desc = *desc;
    s_audio.sample_rate = desc->sample_rate ? desc->sample_rate : 44100;
    s_audio.num_channels = desc->num_channels ? desc->num_channels : 1;

    if (s_audio.num_channels > SAUDIO_MAX_CHANNELS) {
        saudio_log(1, "AudioWorklet backend supports at most 2 channels");
        return;
    }
    /* Push mode only -- see the file header. A stream callback would silently never
       fire, so say so rather than leaving the caller to wonder. */
    if (desc->stream_cb || desc->stream_userdata_cb) {
        saudio_log(1, "AudioWorklet backend is push-mode only, the stream callback is ignored");
    }

    EmscriptenWebAudioCreateAttributes attributes = {
        .latencyHint = "interactive",
        .sampleRate = (uint32_t)s_audio.sample_rate,
        .renderSizeHint = AUDIO_CONTEXT_RENDER_SIZE_DEFAULT,
    };
    s_audio.context = emscripten_create_audio_context(&attributes);
    if (!s_audio.context) {
        saudio_log(1, "WebAudio AudioContext creation failed");
        return;
    }

    /* Both are available synchronously, which src/x65.c depends on: it reads
       saudio_sample_rate() the moment saudio_setup() returns, to decide whether the
       SGU output needs a resampler. */
    s_audio.sample_rate = emscripten_audio_context_sample_rate(s_audio.context);
    s_audio.buffer_frames = emscripten_audio_context_quantum_size(s_audio.context);
    if (s_audio.buffer_frames <= 0) s_audio.buffer_frames = 128;

    /* desc->buffer_frames is the caller's latency request. The AudioWorklet quantum
       is not negotiable, so it becomes the prefill depth here rather than a device
       buffer size -- which is also why saudio_buffer_frames() reports the quantum. */
    s_audio.prefill = desc->buffer_frames > 0 ? (uint32_t)desc->buffer_frames : SAUDIO_DEFAULT_PREFILL_FRAMES;
    if (s_audio.prefill > SAUDIO_FIFO_FRAMES / 2) s_audio.prefill = SAUDIO_FIFO_FRAMES / 2;

    s_write_pos = 0;
    s_read_pos = 0;
    s_priming = 1;

    /* Context creation succeeded. Processor/node creation completes asynchronously.
       Keep valid true even if that later stage reports an error so saudio_shutdown()
       still owns and reclaims the context. */
    s_audio.valid = true;
    emscripten_start_wasm_audio_worklet_thread_async(
        s_audio.context,
        s_worklet_stack,
        sizeof(s_worklet_stack),
        saudio_worklet_started,
        NULL);

    /* These sit on the document, while sokol_app takes mousedown/touchend on the
       canvas and keydown on the window, so neither displaces the other. */
    emscripten_set_click_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, true, saudio_mouse_resume);
    emscripten_set_touchend_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, true, saudio_touch_resume);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, true, saudio_key_resume);
}

void saudio_shutdown(void) {
    if (!s_audio.setup_called) return;

    // stop the worklet entering the queue before tearing the node down
    __atomic_store_n(&s_audio.active, 0, __ATOMIC_RELEASE);

    if (s_audio.node) emscripten_destroy_web_audio_node(s_audio.node);
    if (s_audio.context) emscripten_destroy_audio_context(s_audio.context);

    memset(&s_audio, 0, sizeof(s_audio));
}

bool saudio_isvalid(void) {
    return s_audio.valid;
}
void* saudio_userdata(void) {
    return s_audio.desc.user_data;
}
saudio_desc saudio_query_desc(void) {
    return s_audio.desc;
}
int saudio_sample_rate(void) {
    return s_audio.sample_rate;
}
int saudio_buffer_frames(void) {
    return s_audio.buffer_frames;
}
int saudio_channels(void) {
    return s_audio.num_channels;
}

bool saudio_suspended(void) {
    if (!s_audio.context) return false;
    const AUDIO_CONTEXT_STATE state = emscripten_audio_context_state(s_audio.context);
    return state == AUDIO_CONTEXT_STATE_SUSPENDED || state == AUDIO_CONTEXT_STATE_INTERRUPTED;
}

int saudio_expect(void) {
    if (!s_audio.valid) return 0;
    const uint32_t w = __atomic_load_n(&s_write_pos, __ATOMIC_RELAXED);  // we own it
    const uint32_t r = __atomic_load_n(&s_read_pos, __ATOMIC_ACQUIRE);
    return (int)(SAUDIO_FIFO_FRAMES - (w - r));
}

int saudio_push(const float* frames, int num_frames) {
    if (!s_audio.valid || !frames || num_frames <= 0) return 0;

    const uint32_t channels = (uint32_t)s_audio.num_channels;
    const uint32_t w = __atomic_load_n(&s_write_pos, __ATOMIC_RELAXED);  // we own it
    const uint32_t r = __atomic_load_n(&s_read_pos, __ATOMIC_ACQUIRE);   // pairs with the process release

    uint32_t n = SAUDIO_FIFO_FRAMES - (w - r);
    if (n > (uint32_t)num_frames) n = (uint32_t)num_frames;
    if (n == 0) return 0;  // overrun: drop, the same as upstream's _saudio_fifo_write

    const uint32_t idx = w & (SAUDIO_FIFO_FRAMES - 1);
    uint32_t first = SAUDIO_FIFO_FRAMES - idx;
    if (first > n) first = n;
    memcpy(&s_fifo[(size_t)idx * channels], frames, (size_t)first * channels * sizeof(float));
    if (n > first) {
        memcpy(&s_fifo[0], frames + (size_t)first * channels, (size_t)(n - first) * channels * sizeof(float));
    }

    __atomic_store_n(&s_write_pos, w + n, __ATOMIC_RELEASE);  // publishes the samples
    return (int)n;
}
