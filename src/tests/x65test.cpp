// Whole-machine regressions share the application's core sources and host stubs.
// All guest execution goes through x65_exec; device state is exercised sequentially.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#define CHIPS_IMPL
#include "chips/chips_common.h"
#include "chips/w65c816s.h"
#include "chips/clk.h"
#include "chips/beeper.h"
#undef CHIPS_IMPL

#include "systems/x65.h"
extern "C" {
#include "args.h"
}

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

static const size_t FB_PIXELS = CGIA_FRAMEBUFFER_SIZE_BYTES / 4;
static const int LAST_LINE = MODE_V_TOTAL_LINES - 1;

// Neither value can come out of the renderer: CGIA writes every pixel with the
// alpha byte set, and neither of these has 0xFF on top.
// Nothing the tests wait for is more than a frame away, so two frames is the
// ceiling for every bounded wait here.
static const uint32_t TWO_FRAMES_US = 2 * 1000000 / MODE_V_FREQ_HZ;

static const uint32_t POISON = 0xDEADBEEF;
static const uint32_t CANARY = 0x0BADF00D;

// The machines are far too big for the stack (16 MB of RAM each) and doctest
// runs every case in one process, so keep one of each and re-init per case.
static x65_t machine;
static x65_t other;
static x65_t snapshot;
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
    const uint64_t before = sys->hooks.tick_count;
    const uint32_t ticks = x65_exec(sys, micro_seconds);
    CHECK(ticks == clk_us_to_ticks(X65_FREQUENCY, micro_seconds));
    CHECK(ticks == sys->hooks.tick_count - before);
}

// A debugger step: the hook stops the exec after `ticks` ticks. Returns the
// ticks actually executed.
static uint64_t step(x65_t* sys, uint64_t ticks) {
    const uint64_t before = sys->hooks.tick_count;
    dbg_stopped = false;
    dbg_budget = ticks;
    // 1 us is 3.14 ticks, so a budget of `ticks` us always outlasts the hook
    const uint32_t completed = x65_exec(sys, (uint32_t)ticks);
    CHECK(completed == sys->hooks.tick_count - before);
    dbg_budget = 0;
    return completed;
}

// Advance in full 1 us budgets until `pred` holds. Nothing waited for here is
// more than a frame away, so running out is a failure and not a longer wait.
template<typename Pred>
static void run_until(x65_t* sys, Pred pred) {
    for (uint32_t us = 0; !pred(); us++) {
        if (us >= TWO_FRAMES_US) {
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

// VBI/status addresses from the firmware CGIA register layout. Keeping the
// guest bytes literal makes its bus transactions readable alongside the code.
static constexpr uint16_t CGIA_ENABLE = 0xFF1A;
static constexpr uint16_t CGIA_STATUS = 0xFF1B;

// The buzzer registers and the CGIA register file are file-scope statics in the
// chips, so x65_init() does not clear them and they leak between cases. Any case
// that cares about interrupts or the buzzer starts from here instead of boot().
static void boot_quiet(x65_t* sys, uint32_t* fb) {
    boot(sys, fb);
    mem_wr(sys, 0, CGIA_ENABLE, 0);
    mem_wr(sys, 0, CGIA_STATUS, 0);
    for (int i = 0; i < 4; ++i) mem_wr(sys, 0, X65_IO_BUZZER_BASE + i, 0);
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
    CHECK(x65_exec(&machine, 1000) == 0);
    CHECK(machine.hooks.tick_count == before);
    CHECK(fb_equals_display(&machine));

    // a script breakpoint ends an exec the same way
    poison_display();
    dbg_stopped = false;
    machine.hooks.break_hit = true;
    before = machine.hooks.tick_count;
    CHECK(x65_exec(&machine, 1000) == 0);
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

TEST_CASE("execution budgets with and without a debug callback") {
    boot(&machine, nullptr);
    SUBCASE("debug callback") {}
    SUBCASE("no callback") { machine.debug.callback.func = nullptr; }
    run_us(&machine, 0);
    run_us(&machine, 100);
}

TEST_CASE("debug stop reports completed ticks") {
    boot(&machine, nullptr);
    CHECK(step(&machine, 17) == 17);
}

TEST_CASE("opcode breakpoint counts the detecting fetch") {
    boot(&machine, display_fb);
    // Reset vector -> NOP; JMP $2000. Stop on the first opcode fetch.
    mem_wr16(&machine, 0, 0xFFFC, 0x2000);
    const uint8_t code[] = {0xEA, 0x4C, 0x00, 0x20};
    std::memcpy(machine.ram + 0x2000, code, sizeof(code));
    machine.hooks.break_addr = 0x2000;
    const uint64_t before = machine.hooks.tick_count;
    const uint32_t ticks = x65_exec(&machine, 100);
    CHECK(machine.hooks.break_hit);
    CHECK(ticks > 0);
    CHECK(ticks < clk_us_to_ticks(X65_FREQUENCY, 100));
    CHECK(ticks == machine.hooks.tick_count - before);
    CHECK(W65816_IS_FETCH(machine.pins));
    CHECK(W65816_GET_ADDR(machine.pins) == 0x2000);
    CHECK(fb_equals_display(&machine));
}

static void install_guest(uint16_t address, const std::vector<uint8_t>& code) {
    std::copy(code.begin(), code.end(), machine.ram + address);
}

// Each tiny guest starts via the real reset vector and finishes at an armed
// opcode breakpoint. Devices keep their state across successive bus probes.
static void guest(const std::vector<uint8_t>& instructions) {
    REQUIRE(instructions.size() < 0xF0);
    const w65816_desc_t desc = {};
    machine.pins = w65816_init(&machine.cpu, &desc);
    mem_wr16(&machine, 0, 0xFFFC, 0x2000);
    install_guest(0x2000, instructions);
    const std::vector<uint8_t> finish = {0x4C, 0x00, 0x21}; // JMP $2100
    install_guest(uint16_t(0x2000 + instructions.size()), finish);
    install_guest(0x2100, finish);
    machine.hooks.break_addr = 0x2100;
    machine.hooks.break_hit = false;
    dbg_stopped = false;
    dbg_budget = 0;
    const uint64_t before = machine.hooks.tick_count;
    const uint32_t ticks = x65_exec(&machine, 1000);
    REQUIRE(machine.hooks.break_hit);
    CHECK(ticks == machine.hooks.tick_count - before);
    machine.hooks.break_addr = X65_NO_BREAK_ADDR;
    machine.hooks.break_hit = false;
}

static uint8_t guest_read(uint32_t address) {
    // LDA long address; STA $0300 (8-bit accumulator after reset).
    machine.ram[0x300] = 0;
    guest({0xAF, uint8_t(address), uint8_t(address >> 8), uint8_t(address >> 16),
           0x8D, 0x00, 0x03});
    return machine.ram[0x300];
}

static void guest_write(uint32_t address, uint8_t value) {
    // LDA #value; STA long address.
    guest({0xA9, value, 0x8F, uint8_t(address), uint8_t(address >> 8), uint8_t(address >> 16)});
}

TEST_CASE("timer inspection and CPU access share the X65 register mapping") {
    boot(&machine, nullptr);
    mem_wr16(&machine, 0, 0xFF98, 0x1234);
    mem_wr16(&machine, 0, 0xFF9A, 0x5678);
    CHECK(machine.ria.cia.ta.latch == 0x1234);
    CHECK(machine.ria.cia.tb.latch == 0x5678);
    // Stopped timers load their counters through the normal CIA pipeline.
    guest({0xEA});
    const uint8_t expected[] = {0x34, 0x12, 0x78, 0x56};
    for (int i = 0; i < 4; ++i) {
        CHECK(mem_rd(&machine, 0, 0xFF98 + i) == expected[i]);
        CHECK(guest_read(0xFF98 + i) == expected[i]);
    }
    guest_write(0xFF98, 0xAB);
    guest_write(0xFF99, 0xCD);
    guest_write(0xFF9A, 0xEF);
    guest_write(0xFF9B, 0x12);
    guest({0xEA}); // allow the Timer B high-byte load pipeline to settle
    CHECK(mem_rd16(&machine, 0, 0xFF98) == 0xCDAB);
    CHECK(mem_rd16(&machine, 0, 0xFF9A) == 0x12EF);
    mem_wr(&machine, 0, 0xFF9E, 0x10); // force load is write-only
    mem_wr(&machine, 0, 0xFF9F, 0x10);
    CHECK(mem_rd(&machine, 0, 0xFF9E) == 0);
    CHECK(mem_rd(&machine, 0, 0xFF9F) == 0);
    CHECK(guest_read(0xFF9E) == 0);
    CHECK(guest_read(0xFF9F) == 0);
    guest_write(0xFF9E, 0x08); // one-shot, stopped
    guest_write(0xFF9F, 0x08);
    CHECK(mem_rd(&machine, 0, 0xFF9E) == 0x08);
    CHECK(mem_rd(&machine, 0, 0xFF9F) == 0x08);
}

TEST_CASE("timer inspection preserves interrupt latch and pipeline") {
    boot(&machine, nullptr);
    machine.ria.cia.intr.icr = 0x81;
    machine.ria.cia.intr.pip = 0x1234;
    const auto before = machine.ria.cia;
    const auto us = machine.ria.us;
    for (int i = 0; i < 3; ++i) CHECK(mem_rd(&machine, 0, 0xFF9D) == 0x81);
    CHECK(std::memcmp(&before, &machine.ria.cia, sizeof(before)) == 0);
    CHECK(machine.ria.us == us);
    // A real bus read acknowledges the status. Clear synthetic pipeline bits
    // before executing, so only the seeded latch can contribute interrupt state.
    machine.ria.cia.intr.pip = 0;
    CHECK(guest_read(0xFF9D) == 0x81);
    CHECK(machine.ria.cia.intr.icr == 0);
    CHECK((machine.ria.cia.intr.pip & (0xFFu << M6526_PIP_IRQ)) == 0);
}

TEST_CASE("reserved timer and unused buzzer-adjacent addresses ignore writes") {
    boot(&machine, nullptr);
    const auto before = machine.ria.cia;
    mem_wr(&machine, 0, 0xFF9C, 0xA5);
    CHECK(std::memcmp(&before, &machine.ria.cia, sizeof(before)) == 0);
    CHECK(mem_rd(&machine, 0, 0xFF9C) == 0xFF);
    guest_write(0xFF9C, 0xA5);
    CHECK(guest_read(0xFF9C) == 0xFF);
    CHECK(machine.ria.cia.ta.latch == before.ta.latch);
    CHECK(machine.ria.cia.tb.latch == before.tb.latch);
    CHECK(machine.ria.cia.intr.imr1 == before.intr.imr1);
    for (int i = 0; i < 4; ++i) mem_wr(&machine, 0, 0xFFA8 + i, 0x20 + i);
    for (int unused = 0xFFAC; unused <= 0xFFAF; ++unused) {
        mem_wr(&machine, 0, unused, 0xA5);
        guest_write(unused, 0x5A);
        CHECK(mem_rd(&machine, 0, unused) == 0xFF);
        for (int i = 0; i < 4; ++i) CHECK(mem_rd(&machine, 0, 0xFFA8 + i) == 0x20 + i);
    }
    for (int i = 0; i < 4; ++i) {
        guest_write(0xFFA8 + i, 0x30 + i);
        CHECK(mem_rd(&machine, 0, 0xFFA8 + i) == 0x30 + i);
        mem_wr(&machine, 0, 0xFFA8 + i, 0); // static buzzer cleanup
    }
}

TEST_CASE("SGU service inspection preserves status and sample offsets") {
    boot(&machine, nullptr);
    mem_wr(&machine, 0, 0xFEFF, SGU1_SERVICE_BANK);
    machine.sgu.svc_status = 0x101;
    for (int i = 0; i < 3; ++i) CHECK(mem_rd(&machine, 0, 0xFED0) == 1);
    CHECK(machine.sgu.svc_status == 0x101);
    CHECK(guest_read(0xFED0) == 1);
    CHECK(machine.sgu.svc_status == 0x100);
    CHECK(guest_read(0xFED0) == 0);

    // Upload across the 16-bit offset wrap, keeping the PCM bank fixed.
    mem_wr(&machine, 0, 0xFEDE, 1);
    mem_wr16(&machine, 0, 0xFEDC, 0xFFFF);
    mem_wr(&machine, 0, 0xFEDF, 0xA5);
    mem_wr(&machine, 0, 0xFEDF, 0x5A);
    mem_wr16(&machine, 0, 0xFEDC, 0xFFFF);
    for (int i = 0; i < 3; ++i) CHECK(mem_rd(&machine, 0, 0xFEDF) == 0xA5);
    CHECK(machine.sgu.svc_sample_offset == 0xFFFF);
    CHECK(guest_read(0xFEDF) == 0xA5);
    CHECK(machine.sgu.svc_sample_offset == 0);
    CHECK(mem_rd(&machine, 0, 0xFEDF) == 0x5A);
    CHECK(guest_read(0xFEDF) == 0x5A);
    CHECK(machine.sgu.svc_sample_offset == 1);
    CHECK(machine.sgu.svc_sample_bank == 1);
}

TEST_CASE("SGU selected-window inspection retains channels and diagnostics") {
    boot(&machine, nullptr);
    for (uint8_t selection : {uint8_t(2), uint8_t(SGU1_SERVICE_BANK), uint8_t(0xFE)}) {
        mem_wr(&machine, 0, 0xFEFF, selection);
        CHECK(mem_rd(&machine, 0, 0xFEFF) == selection);
        CHECK(guest_read(0xFEFF) == selection);
    }
    CHECK(mem_rd(&machine, 0, 0xFEC0) == 0xFF);
    CHECK(guest_read(0xFEC0) == 0xFF);
    mem_wr(&machine, 0, 0xFEFF, 2);
    mem_wr(&machine, 0, 0xFEC0, 0x37);
    CHECK(mem_rd(&machine, 0, 0xFEC0) == 0x37);
    CHECK(guest_read(0xFEC0) == 0x37);
    mem_wr(&machine, 0, X65_IO_SGU_BASE + SGU_PATCH_CHN(SGU1_CHN_FLAGS1), SGU1_FLAGS1_DIAG);
    machine.sgu.sgu.src[2] = 0x1234;
    CHECK(mem_rd(&machine, 0, 0xFEE0) == 0x34);
    CHECK(mem_rd(&machine, 0, 0xFEE1) == 0x12);
    // CPU probe must finish before the next audio sample overwrites src[].
    machine.sgu.tick_counter = machine.sgu.tick_period;
    CHECK(guest_read(0xFEE0) == 0x34);
}

extern "C" {
    extern uint8_t xstack[];
    extern volatile size_t xstack_ptr;
}

TEST_CASE("RIA FIFO and API-stack inspection exceptions remain intact") {
    boot(&machine, nullptr);
    REQUIRE(rb_put(&machine.ria.uart_rx, 0x62));
    const auto fifo = machine.ria.uart_rx;
    const auto saved_ptr = xstack_ptr;
    const auto saved_byte = xstack[10];
    xstack_ptr = 10;
    xstack[10] = 0x73;
    for (int i = 0; i < 3; ++i) {
        for (uint16_t address : {0xFFDC, 0xFFDD, 0xFFE1, 0xFFF2})
            CHECK(mem_rd(&machine, 0, address) == 0xFF);
    }
    CHECK(std::memcmp(&fifo, &machine.ria.uart_rx, sizeof(fifo)) == 0);
    CHECK(xstack_ptr == 10);
    CHECK(guest_read(0xFFE1) == 0x62);
    CHECK(rb_is_empty(&machine.ria.uart_rx));
    CHECK(guest_read(0xFFF2) == 0x73);
    CHECK(xstack_ptr == 11);
    xstack_ptr = saved_ptr;
    xstack[10] = saved_byte;
}

TEST_CASE("device windows select chips only in bank zero") {
    boot_quiet(&machine, nullptr);
    // Writable, stable representatives: RIA operand, RGB, buzzer, timer A,
    // GPIO output, CGIA background colour, SGU operator register.
    const uint16_t windows[] = {0xFFC0, 0xFFA0, 0xFFA8, 0xFF98, 0xFF82, 0xFF34, 0xFEC0};
    mem_wr(&machine, 0, 0xFEFF, 0);
    for (uint16_t address : windows) {
        CAPTURE(address);
        guest_write(address, 0x35);
        if (address == 0xFF98) guest_write(0xFF99, 0); // high byte loads stopped timer
        guest({0xEA}); // timer load pipeline
        CHECK(guest_read(address) == 0x35);
        mem_wr(&machine, 1, address, 0x72);
        CHECK(guest_read(0x10000u + address) == 0x72);
        guest_write(0x10000u + address, 0xA6);
        CHECK(mem_rd(&machine, 1, address) == 0xA6);
        CHECK(guest_read(address) == 0x35);
    }
    // HID selection is write-only for some banks; inspect the selection itself.
    guest_write(0xFFB0, 0x0F); // unsupported HID device, reads FF
    CHECK(ria816_hid_dev(&machine.ria) == 0x0F);
    CHECK(guest_read(0xFFB0) == 0xFF);
    mem_wr(&machine, 1, 0xFFB0, 0x72);
    CHECK(guest_read(0x1FFB0) == 0x72);
    guest_write(0x1FFB0, 0xA6);
    CHECK(mem_rd(&machine, 1, 0xFFB0) == 0xA6);
    CHECK(ria816_hid_dev(&machine.ria) == 0x0F);
    mem_wr(&machine, 0, 0xFFB0, 0);
    for (int i = 0; i < 4; ++i) mem_wr(&machine, 0, 0xFFA8 + i, 0);
}

TEST_CASE("each expansion chunk independently routes CPU accesses") {
    boot(&machine, nullptr);
    for (uint8_t bitmap : {0x00, 0xFF, 0xA5}) {
        mem_wr(&machine, 0, 0xFFF6, bitmap);
        for (unsigned chunk = 0; chunk < 8; ++chunk) {
            for (unsigned edge : {0u, unsigned(X65_EXT_CHUNK_LEN - 1)}) {
                const uint16_t address = X65_EXT_BASE + chunk * X65_EXT_CHUNK_LEN + edge;
                const bool ram = (bitmap & (1u << chunk)) != 0;
                CAPTURE(bitmap);
                CAPTURE(chunk);
                CAPTURE(edge);
                mem_wr(&machine, 0, address, 0x31); // debugger bypass
                CHECK(mem_ram_read(&machine, address) == 0x31);
                CHECK(mem_rd(&machine, 0, address) == 0x31);
                CHECK(guest_read(address) == (ram ? 0x31 : 0xFF));
                guest_write(address, 0x62);
                CHECK(mem_ram_read(&machine, address) == (ram ? 0x62 : 0x31));
                mem_ram_write(&machine, address, 0x94);
                CHECK(mem_rd(&machine, 0, address) == 0x94);
                CHECK(guest_read(address) == (ram ? 0x94 : 0xFF));
            }
        }
    }
}


TEST_CASE("Timer A one-shot wakes WAI through CIA and the RIA IRQ gate") {
    boot_quiet(&machine, nullptr);
    uint8_t gate = 1;
    SUBCASE("gate enabled") {}
    SUBCASE("gate disabled") { gate = 0; }
    mem_wr16(&machine, 0, 0xFFFC, 0x2000);
    mem_wr16(&machine, 0, 0xFFFE, 0x2200);
    machine.ram[0x300] = machine.ram[0x301] = 0;
    install_guest(0x2000, {
        0x78, 0xD8, 0xA2, 0xFF, 0x9A, // SEI; CLD; LDX #FF; TXS
        0xA9, 0x64, 0x8D, 0x98, 0xFF, // Timer A interval = 100 us
        0xA9, 0x00, 0x8D, 0x99, 0xFF,
        0xA9, 0x81, 0x8D, 0x9D, 0xFF, // enable CIA Timer A mask
        0xA9, gate, 0x8D, 0xEC, 0xFF, // RIA timer interrupt gate
        0xA9, 0x19, 0x8D, 0x9E, 0xFF, // force load, one-shot, start
        0x58,                         // CLI
        0xCB, 0x80, 0xFD,             // WAI; BRA back to WAI
    });
    install_guest(0x2200, {
        0xAD, 0x9D, 0xFF, // LDA ICR: capture and acknowledge via CPU bus
        0x8D, 0x01, 0x03, // STA $0301
        0xEE, 0x00, 0x03, // INC $0300
        0x40,             // RTI
    });
    const auto before = machine.hooks.tick_count;
    run_us(&machine, 2000);
    CHECK(machine.hooks.tick_count - before == clk_us_to_ticks(X65_FREQUENCY, 2000));
    CHECK(machine.ram[0x300] == gate);
    CHECK(machine.cpu.stopped == W65816_STOP_WAI);
    CHECK((machine.ria.cia.ta.cr & 1) == 0);
    if (gate) {
        CHECK(machine.ram[0x301] == 0x81);
        CHECK(machine.ria.cia.intr.icr == 0);
        CHECK((machine.ria.cia.pins & M6526_IRQ) == 0);
    } else {
        CHECK(mem_rd(&machine, 0, 0xFF9D) == 0x81);
        CHECK((machine.ria.cia.pins & M6526_IRQ) != 0);
    }
    CHECK((machine.ria.pins & RIA816_IRQ) == 0);
    CHECK(machine.ria.int_status == 0);
    CHECK((machine.pins & W65816_IRQ) == 0);
    run_us(&machine, 2000);
    CHECK(machine.ram[0x300] == gate);
}

TEST_CASE("CGIA VBI delivers NMI and requires a status write to acknowledge") {
    boot_quiet(&machine, nullptr);
    // Clear static interrupt state even if a REQUIRE aborts this case.
    struct Cleanup {
        ~Cleanup() {
            mem_wr(&machine, 0, CGIA_ENABLE, 0);
            mem_wr(&machine, 0, CGIA_STATUS, 0);
            step(&machine, 4);
        }
    } cleanup;
    mem_wr16(&machine, 0, 0xFFFC, 0x2000);
    mem_wr16(&machine, 0, 0xFFFA, 0x2200);
    machine.ram[0x300] = machine.ram[0x301] = machine.ram[0x302] = 0;
    install_guest(0x2000, {
        0x78, 0xD8, 0xA2, 0xFF, 0x9A, // stack/interrupt setup
        0xA9, 0x80, 0x8D, 0x1A, 0xFF, // enable VBI only
        0xCB, 0x80, 0xFD,             // WAI; BRA back to WAI
    });
    install_guest(0x2200, {
        0xAD, 0x1B, 0xFF, 0x8D, 0x01, 0x03, // read status -> $0301
        0xAD, 0x1B, 0xFF, 0x8D, 0x02, 0x03, // read again -> $0302
        0xEE, 0x00, 0x03,                   // INC handler counter
        0xEA,                               // breakpoint before acknowledgement
        0x8D, 0x1B, 0xFF,                   // STA status: acknowledge
        0x40,                               // RTI
    });
    machine.hooks.break_addr = 0x220F;
    // At most two emulated frames per wait; x65_exec owns every device tick.
    x65_exec(&machine, TWO_FRAMES_US);
    REQUIRE(machine.hooks.break_hit);
    CHECK(machine.ram[0x300] == 1);
    CHECK(machine.ram[0x301] == 0x80);
    CHECK(machine.ram[0x302] == 0x80);
    CHECK(mem_rd(&machine, 0, CGIA_STATUS) == 0x80);
    CHECK((machine.cgia.pins & CGIA_INT) != 0);
    CHECK((machine.pins & W65816_NMI) != 0);
    machine.hooks.break_hit = false;
    machine.hooks.break_addr = X65_NO_BREAK_ADDR;
    run_us(&machine, 20);
    CHECK(mem_rd(&machine, 0, CGIA_STATUS) == 0);
    CHECK((machine.cgia.pins & CGIA_INT) == 0);
    CHECK((machine.pins & W65816_NMI) == 0);
    run_until(&machine, [] { return machine.ram[0x300] == 2; });
    run_us(&machine, 20); // finish the second handler
    CHECK(machine.ram[0x300] == 2);
    guest_write(CGIA_ENABLE, 0);
    // Park the probe guest in a stable loop before the long disabled-source wait.
    install_guest(0x2100, {0x4C, 0x00, 0x21});
    run_us(&machine, TWO_FRAMES_US);
    CHECK(machine.ram[0x300] == 2);
    CHECK((machine.pins & W65816_NMI) == 0);
}

struct AudioCapture {
    std::vector<float> samples;
    size_t target;
};

static void capture_audio(const float* samples, int count, void* user_data) {
    auto* capture = static_cast<AudioCapture*>(user_data);
    capture->samples.insert(capture->samples.end(), samples, samples + count);
    if (capture->samples.size() >= capture->target) dbg_stopped = true;
}

static std::vector<float> capture_samples(AudioCapture& capture, size_t stereo_samples) {
    capture.samples.clear();
    capture.target = stereo_samples * 2;
    dbg_stopped = false;
    dbg_budget = 0;
    x65_exec(&machine, 10000); // comfortably exceeds 128 samples at 48 kHz
    REQUIRE(dbg_stopped);
    REQUIRE(capture.samples.size() == capture.target);
    CHECK(machine.audio.sample_pos == 0);
    return capture.samples;
}

TEST_CASE("machine snapshots continue looping PCM at the saved sample and phase") {
    boot_quiet(&machine, nullptr);
    mem_wr16(&machine, 0, 0xFFFC, 0x2000);
    install_guest(0x2000, {0x78, 0x4C, 0x01, 0x20}); // SEI; JMP $2001
    for (unsigned ch = 0; ch < SGU_CHNS; ++ch) {
        mem_wr(&machine, 0, 0xFEFF, ch);
        mem_wr(&machine, 0, 0xFEC0 + SGU_PATCH_CHN(SGU1_CHN_VOL), 0);
    }
    const int8_t pcm[] = {63, -41, 112, -96, 28, -17, 77};
    mem_wr(&machine, 0, 0xFEFF, SGU1_SERVICE_BANK);
    mem_wr16(&machine, 0, 0xFEDC, 0);
    mem_wr(&machine, 0, 0xFEDE, 0);
    for (int8_t value : pcm) mem_wr(&machine, 0, 0xFEDF, uint8_t(value));
    mem_wr(&machine, 0, 0xFEE0, 0xFF); // service master volume
    mem_wr(&machine, 0, 0xFEFF, 0);
    const uint16_t channel = 0xFEC0 + SGU_PATCH_CHN(0);
    mem_wr16(&machine, 0, channel + SGU1_CHN_FREQ_L, 0x2345);
    mem_wr(&machine, 0, channel + SGU1_CHN_VOL, 0x60);
    mem_wr(&machine, 0, channel + SGU1_CHN_PAN, 0);
    mem_wr16(&machine, 0, channel + SGU1_CHN_PCM_POS_L, 0);
    mem_wr16(&machine, 0, channel + SGU1_CHN_PCM_END_L, sizeof(pcm));
    mem_wr16(&machine, 0, channel + SGU1_CHN_PCM_RST_L, 0);
    mem_wr(&machine, 0, channel + SGU1_CHN_FLAGS1, SGU1_FLAGS1_PCM_LOOP);
    mem_wr(&machine, 0, channel + SGU1_CHN_FLAGS0, SGU1_FLAGS0_PCM_MASK | SGU1_FLAGS0_CTL_GATE);
    AudioCapture capture = {};
    machine.audio.num_samples = 2; // one stereo sample per callback
    machine.audio.callback = {capture_audio, &capture};
    capture_samples(capture, 64);
    const uint32_t version = x65_save_snapshot(&machine, &snapshot);
    const auto saved_pos = machine.sgu.sgu.chan[0].pcmpos;
    const auto saved_phase = machine.sgu.sgu.pcm_phase_accum[0];
    const auto saved_counter = machine.sgu.sgu.sample_counter;
    CHECK(saved_phase != 0);
    CHECK(saved_counter == 64);
    const auto expected = capture_samples(capture, 128);
    REQUIRE(std::any_of(expected.begin(), expected.end(), [](float sample) { return sample != 0; }));
    const auto end_pos = machine.sgu.sgu.chan[0].pcmpos;
    const auto end_phase = machine.sgu.sgu.pcm_phase_accum[0];
    const auto end_counter = machine.sgu.sgu.sample_counter;
    std::fill(std::begin(machine.sgu.pcm), std::end(machine.sgu.pcm), 0);
    machine.sgu.sgu.chan[0] = {};
    machine.sgu.sgu.pcm_phase_accum[0] = 0;
    machine.sgu.sgu.sample_counter = 999;
    REQUIRE(x65_load_snapshot(&machine, version, &snapshot));
    CHECK(machine.audio.callback.func == capture_audio);
    CHECK(machine.audio.callback.user_data == &capture);
    CHECK(machine.sgu.sgu.pcm == machine.sgu.pcm);
    CHECK(std::memcmp(machine.sgu.pcm, pcm, sizeof(pcm)) == 0);
    CHECK(machine.sgu.sgu.chan[0].pcmpos == saved_pos);
    CHECK(machine.sgu.sgu.pcm_phase_accum[0] == saved_phase);
    CHECK(machine.sgu.sgu.sample_counter == saved_counter);
    CHECK(capture_samples(capture, 128) == expected);
    CHECK(machine.sgu.sgu.chan[0].pcmpos == end_pos);
    CHECK(machine.sgu.sgu.pcm_phase_accum[0] == end_phase);
    CHECK(machine.sgu.sgu.sample_counter == end_counter);
    machine.audio.callback = {};
}

// Restore process arguments even if an assertion aborts a seeded case.
struct RestoreArguments {
    struct arguments saved = arguments;
    ~RestoreArguments() { arguments = saved; }
};

static std::array<uint8_t, 32> guest_rng_sequence() {
    // LDX #0; LDA RNG; STA $0320,X; INX; CPX #32; BNE loop.
    guest({0xA2, 0x00, 0xAD, 0xE2, 0xFF, 0x9D, 0x20, 0x03,
           0xE8, 0xE0, 0x20, 0xD0, 0xF5});
    std::array<uint8_t, 32> bytes;
    std::copy_n(machine.ram + 0x320, bytes.size(), bytes.begin());
    return bytes;
}

TEST_CASE("seed reproduces RAM and guest RNG independently of zero-mem") {
    RestoreArguments restore;
    arguments.seed_supplied = true;
    arguments.seed = 0; // zero is a supplied seed too
    arguments.zeromem = false;
    boot(&machine, nullptr);
    // Retain the complete randomized RAM before guest code changes it.
    std::memcpy(other.ram, machine.ram, sizeof(machine.ram));
    const auto first = guest_rng_sequence();
    boot(&machine, nullptr);
    CHECK(std::memcmp(other.ram, machine.ram, sizeof(machine.ram)) == 0);
    CHECK(guest_rng_sequence() == first);
    arguments.zeromem = true;
    boot(&machine, nullptr);
    CHECK(std::all_of(std::begin(machine.ram), std::end(machine.ram), [](uint8_t b) { return b == 0; }));
    CHECK(guest_rng_sequence() == first);
}

TEST_CASE("full initialization restarts the seed while ordinary reset continues it") {
    RestoreArguments restore;
    arguments.seed_supplied = true;
    arguments.seed = 0x12345678;
    arguments.zeromem = true;
    boot(&machine, nullptr);
    const auto first = guest_rng_sequence();
    const auto second = guest_rng_sequence();
    CHECK(first != second);
    boot(&machine, nullptr);
    CHECK(guest_rng_sequence() == first);
    x65_reset(&machine);
    CHECK(guest_rng_sequence() == second);
}

// Sprite pixel formats and the per-sprite 16-entry palette: entries 0..3 come
// from the descriptor, 4..11 from the sprite plane's color registers, 12..15
// are the descriptor colors half-bright. Register offsets follow the firmware's
// struct cgia_t; every byte the CGIA reads goes through mem_wr so the VRAM
// cache mirrors it (install_guest writes the RAM array directly and would not).
TEST_CASE("sprites of every depth resolve through the combined palette") {
    RestoreArguments restore;
    arguments.zeromem = true;
    boot_quiet(&machine, display_fb);

    // park the CPU on STP so nothing scribbles over the sprite data
    const w65816_desc_t desc = {};
    machine.pins = w65816_init(&machine.cpu, &desc);
    mem_wr16(&machine, 0, 0xFFFC, 0x2000);
    mem_wr(&machine, 0, 0x2000, 0xDB);

    // descriptor colors: level bit 2 clear, so the half-bright copies differ
    const uint8_t dsc_colors[4] = { 0x21, 0x32, 0x43, 0x51 };
    const uint8_t plane_colors[8] = { 0x61, 0x62, 0x63, 0x68, 0x71, 0x72, 0x73, 0x78 };
    const uint8_t back = 0x07;
    uint8_t palette[16];
    for (int i = 0; i < 4; ++i) {
        palette[i] = dsc_colors[i];
        palette[12 + i] = dsc_colors[i] ^ 0b100;
    }
    for (int i = 0; i < 8; ++i)
        palette[4 + i] = plane_colors[i];

    constexpr uint16_t DSC = 0x4000;   // descriptor table, 16 bytes per sprite
    constexpr uint16_t DATA = 0x5000;  // 64 bytes of pixel data per sprite
    constexpr int16_t X = 16;

    // flags: bits 0-2 columns-1, bit 3 double width, bits 4-5 depth, bit 6 mirror X
    struct Sprite {
        uint8_t flags;
        uint16_t lines;
        std::vector<uint8_t> data;
    };
    const std::vector<uint8_t> ramp3 = { 0x29, 0xCB, 0xB8 };                                // 3bpp: 1..7 0
    const std::vector<uint8_t> ramp3_down = { 0xFA, 0xC6, 0x88 };                           // 3bpp: 7..1 0
    const std::vector<uint8_t> ramp4 = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 };  // 4bpp: 1..15 0
    auto lines = [](std::vector<uint8_t> a, const std::vector<uint8_t>& b) {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    };
    const std::vector<Sprite> sprites = {
        { 0b00000000, 1, { 0b10100000 } }, // 1bpp
        { 0b00010000, 1, { 0x6C, 0x6C } }, // 2bpp: 1 2 3 0 1 2 3 0
        { 0b00100000, 2, lines(ramp3, ramp3_down) }, // 3bpp, two lines
        { 0b00110001, 1, ramp4 }, // 4bpp, 2 columns
        { 0b01110001, 1, ramp4 }, // 4bpp mirrored
        { 0b00101000, 1, ramp3 }, // 3bpp doubled
    };
    for (size_t i = 0; i < sprites.size(); ++i) {
        const uint16_t dsc = DSC + (uint16_t)(16 * i);
        const uint16_t data = DATA + (uint16_t)(64 * i);
        const int16_t y = (int16_t)(10 + 10 * i);
        mem_wr16(&machine, 0, dsc + 0, (uint16_t)X);
        mem_wr16(&machine, 0, dsc + 2, (uint16_t)y);
        mem_wr16(&machine, 0, dsc + 4, sprites[i].lines);
        mem_wr(&machine, 0, dsc + 6, sprites[i].flags);
        mem_wr(&machine, 0, dsc + 7, 0);
        for (int c = 0; c < 4; ++c)
            mem_wr(&machine, 0, dsc + 8 + (uint16_t)c, dsc_colors[c]);
        mem_wr16(&machine, 0, dsc + 12, data);
        mem_wr16(&machine, 0, dsc + 14, dsc);
        for (size_t b = 0; b < sprites[i].data.size(); ++b) {
            mem_wr(&machine, 0, data + (uint16_t)b, sprites[i].data[b]);
        }
    }

    mem_wr(&machine, 0, 0xFF01, 0);      // bckgnd_bank
    mem_wr(&machine, 0, 0xFF02, 0);      // sprite_bank
    mem_wr(&machine, 0, 0xFF34, back);   // back_color
    mem_wr16(&machine, 0, 0xFF38, DSC);  // offset[0]
    mem_wr(&machine, 0, 0xFF41, 0);      // plane 0 border_columns
    mem_wr(&machine, 0, 0xFF42, 0);      // plane 0 start_y
    mem_wr(&machine, 0, 0xFF43, 0);      // plane 0 stop_y
    for (int i = 0; i < 8; ++i)
        mem_wr(&machine, 0, 0xFF48 + i, plane_colors[i]);
    mem_wr(&machine, 0, 0xFF40, (uint8_t)((1u << sprites.size()) - 1));  // plane 0 active sprites
    mem_wr(&machine, 0, 0xFF30, 0x11);                                   // plane 0: sprite type, enabled

    run_to_boundary(&machine);
    run_to_boundary(&machine);

    auto rgb = [](uint8_t color) { return machine.cgia.hwcolors[color] | 0xFF000000u; };
    auto px = [](int x, int y) { return display_fb[(2 * y) * CGIA_FRAMEBUFFER_WIDTH + 2 * x]; };
    // the line of sprite i, as palette entries, transparent shown as back_color
    auto expect_line = [&](size_t i, int line, const std::vector<uint8_t>& entries) {
        const int y = 10 + 10 * (int)i + line;
        CAPTURE(i);
        CAPTURE(line);
        for (size_t x = 0; x < entries.size(); ++x) {
            CAPTURE(x);
            const uint8_t color = entries[x] ? palette[entries[x]] : back;
            CHECK(px(X + (int)x, y) == rgb(color));
        }
        CHECK(px(X - 1, y) == rgb(back));
    };

    expect_line(0, 0, { 1, 0, 1, 0, 0, 0, 0, 0 });
    expect_line(1, 0, { 1, 2, 3, 0, 1, 2, 3, 0 });
    expect_line(2, 0, { 1, 2, 3, 4, 5, 6, 7, 0 });
    expect_line(2, 1, { 7, 6, 5, 4, 3, 2, 1, 0 });
    expect_line(3, 0, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0 });
    expect_line(4, 0, { 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 });
    expect_line(5, 0, { 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 0, 0 });
    // a one-line sprite draws nothing on the next line
    CHECK(px(X, 11) == rgb(back));

    mem_wr(&machine, 0, 0xFF30, 0);  // the register file outlives this case
}
