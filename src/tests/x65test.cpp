// Frame presentation: x65_t.fb is the raster CGIA draws into and is torn for
// most of a frame; x65_t.display_fb is the image the host presents. These cases
// pin down the two moments the second one is written -- CGIA completing a frame,
// and an exec the debugger cut short -- and that nothing else ever touches it.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "chips/w65c816s.h"
#include "chips/clk.h"
#include "chips/beeper.h"
#undef CHIPS_IMPL

#include "systems/x65.h"

#include <algorithm>
#include <cstring>

static const size_t FB_PIXELS = CGIA_FRAMEBUFFER_SIZE_BYTES / 4;
static const int LAST_LINE = MODE_V_TOTAL_LINES - 1;

// Neither value can come out of the renderer: CGIA writes every pixel with the
// alpha byte set, and neither of these has 0xFF on top.
static const uint32_t POISON = 0xDEADBEEF;
static const uint32_t CANARY = 0x0BADF00D;

// The machines are far too big for the stack (16 MB of RAM each) and doctest
// runs every case in one process, so keep one of each and re-init per case.
static x65_t machine;
static x65_t other;
static uint32_t display_fb[FB_PIXELS];
static uint32_t last_published[FB_PIXELS];

// The debug hook wears two hats. It stops an exec after `dbg_budget` ticks,
// which is exactly what a debugger step looks like to x65_exec(); and while
// `watched` is armed it counts the copies landing in the display buffer, taking
// each one aside and re-poisoning the canary slot so the next one shows up too.
static bool dbg_stopped;
static uint64_t dbg_budget;
static uint32_t* watched;
static uint32_t publishes;

static void dbg_cb(void*, uint64_t) {
    if (watched && watched[0] != CANARY) {
        publishes++;
        std::memcpy(last_published, watched, sizeof(display_fb));
        watched[0] = CANARY;
    }
    if (dbg_budget > 0 && --dbg_budget == 0) {
        dbg_stopped = true;
    }
}

static void boot(x65_t* sys, uint32_t* fb) {
    dbg_stopped = false;
    dbg_budget = 0;
    watched = nullptr;
    x65_desc_t desc = {};
    desc.debug.callback.func = dbg_cb;
    desc.debug.stopped = &dbg_stopped;
    if (fb) {
        desc.display_framebuffer = { .ptr = fb, .size = CGIA_FRAMEBUFFER_SIZE_BYTES };
    }
    x65_init(sys, &desc);
    x65_set_running(sys, true);
}

// A full budget, with nothing to cut it short: the continuous-execution regime,
// where the only write to the display buffer is a completed frame.
static void run_us(x65_t* sys, uint32_t micro_seconds) {
    dbg_stopped = false;
    dbg_budget = 0;
    x65_exec(sys, micro_seconds);
}

// A debugger step: the hook stops the exec after `ticks` ticks. Returns the
// ticks actually executed.
static uint64_t step(x65_t* sys, uint64_t ticks) {
    const uint64_t before = sys->hooks.tick_count;
    dbg_stopped = false;
    dbg_budget = ticks;
    // 1 us is 3.14 ticks, so a budget of `ticks` us always outlasts the hook
    x65_exec(sys, (uint32_t)ticks);
    dbg_budget = 0;
    return sys->hooks.tick_count - before;
}

// Advance in full 1 us budgets until `pred` holds. Nothing waited for here is
// more than a frame away, so running out is a failure and not a longer wait.
template<typename Pred>
static void run_until(x65_t* sys, Pred pred) {
    const uint32_t timeout_us = 2 * 1000000 / MODE_V_FREQ_HZ;
    for (uint32_t us = 0; !pred(); us++) {
        if (us >= timeout_us) {
            FAIL("timed out waiting for a CGIA state");
            return;
        }
        run_us(sys, 1);
    }
}

static void run_to_boundary(x65_t* sys) {
    const uint32_t frames = sys->cgia.frame_count;
    run_until(sys, [sys, frames] { return sys->cgia.frame_count != frames; });
}

static void poison_display(void) {
    std::fill(display_fb, display_fb + FB_PIXELS, POISON);
}

static bool display_holds(uint32_t value) {
    return std::all_of(display_fb, display_fb + FB_PIXELS, [value](uint32_t px) { return px == value; });
}

static bool fb_equals_display(const x65_t* sys) {
    return std::memcmp(sys->display_fb, sys->fb, sizeof(sys->fb)) == 0;
}

TEST_CASE("booting clears the display buffer") {
    poison_display();
    boot(&machine, display_fb);

    // the host array outlives the machine, so a reboot must not leave the old
    // image on screen until the first frame is published
    CHECK(machine.display_fb == display_fb);
    CHECK(display_holds(0));
}

TEST_CASE("a completed frame is published at the frame boundary") {
    boot(&machine, display_fb);
    // well into the visible part of a frame, so the poison below can only be
    // overwritten by the boundary still ahead of us
    run_until(&machine, [] { return machine.cgia.scan_line > 100; });
    poison_display();

    run_to_boundary(&machine);

    CHECK(machine.cgia.v_count == 0);    // every visible line has been written
    CHECK(fb_equals_display(&machine));  // and the whole frame was published
}

TEST_CASE("nothing but a frame boundary writes the display buffer") {
    boot(&machine, display_fb);
    run_to_boundary(&machine);
    poison_display();

    // most of a frame, stopping short of the next boundary, with nothing
    // cutting the exec short
    const uint32_t frames = machine.cgia.frame_count;
    run_us(&machine, 16000);  // 50240 of the frame's 52333 ticks
    REQUIRE(machine.cgia.frame_count == frames);
    REQUIRE(machine.cgia.scan_line > 0);  // the raster really did move

    CHECK(display_holds(POISON));
}

TEST_CASE("a 48 ms exec publishes every boundary it crosses") {
    boot(&machine, display_fb);
    // park on the last line of a frame: the host's 48 ms clamp is 2.88 emulated
    // frames, so from here a single exec spans three boundaries
    run_until(&machine, [] { return machine.cgia.v_count == LAST_LINE; });

    const uint32_t frames = machine.cgia.frame_count;
    publishes = 0;
    display_fb[0] = CANARY;
    watched = display_fb;
    run_us(&machine, 48000);
    watched = nullptr;

    // every boundary copies, and only the last one is ever presented
    CHECK(publishes == 3);
    CHECK(machine.cgia.frame_count == frames + 3);

    // The exec ran on for another frame after that last boundary, drawing into
    // fb[] the whole way, and the display buffer did not follow: it still holds
    // the frame published at the boundary, canary slot aside.
    CHECK(std::memcmp(display_fb + 1, last_published + 1, sizeof(display_fb) - sizeof(uint32_t)) == 0);
}

TEST_CASE("an exec that stops publishes the raster it stopped on") {
    boot(&machine, display_fb);
    run_until(&machine, [] { return machine.cgia.scan_line > 100; });

    // a step is an exec the debugger cuts short one tick in
    poison_display();
    CHECK(step(&machine, 1) == 1);
    CHECK(fb_equals_display(&machine));  // the half-drawn raster, right away

    // an already-stopped machine runs no ticks and still publishes, which is
    // what a paused debugger does on every host frame
    poison_display();
    dbg_stopped = true;
    uint64_t before = machine.hooks.tick_count;
    x65_exec(&machine, 1000);
    CHECK(machine.hooks.tick_count == before);
    CHECK(fb_equals_display(&machine));

    // a script breakpoint ends an exec the same way
    poison_display();
    dbg_stopped = false;
    machine.hooks.break_hit = true;
    before = machine.hooks.tick_count;
    x65_exec(&machine, 1000);
    machine.hooks.break_hit = false;
    CHECK(machine.hooks.tick_count == before);
    CHECK(fb_equals_display(&machine));
}

TEST_CASE("no display buffer changes nothing") {
    boot(&machine, display_fb);
    run_to_boundary(&machine);
    run_us(&machine, 3000);
    const uint64_t tick_count = machine.hooks.tick_count;

    boot(&other, nullptr);
    CHECK(other.display_fb == nullptr);
    run_to_boundary(&other);
    run_us(&other, 3000);
    CHECK(other.hooks.tick_count == tick_count);
    CHECK(std::memcmp(other.fb, machine.fb, sizeof(other.fb)) == 0);

    // and what each machine offers the host to present
    CHECK(x65_display_info(&other).frame.buffer.ptr == other.fb);
    CHECK(x65_display_info(&machine).frame.buffer.ptr == display_fb);
}

TEST_CASE("snapshots carry the host's buffer, not the saved one") {
    boot(&machine, display_fb);
    run_to_boundary(&machine);
    run_us(&machine, 3000);  // half-drawn raster in fb[]
    REQUIRE(machine.cgia.scan_line > 0);

    static x65_t snapshot;
    const uint32_t version = x65_save_snapshot(&machine, &snapshot);
    CHECK(version == X65_SNAPSHOT_VERSION);
    // a saved x65_t goes into an eight-slot array; it must not carry a pointer
    // into the live host's presentation buffer
    CHECK(snapshot.display_fb == nullptr);

    boot(&other, display_fb);
    REQUIRE(other.display_fb == display_fb);
    CHECK(x65_load_snapshot(&other, version, &snapshot));
    CHECK(other.display_fb == display_fb);  // the live pointer survives the load
    CHECK(other.cgia.v_count == machine.cgia.v_count);
    CHECK(std::memcmp(other.fb, machine.fb, sizeof(other.fb)) == 0);
}
