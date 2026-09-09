#include "./script.h"
#include "./log.h"
#include "systems/x65.h"
#include "chips/ria816.h"

#include <SDL3/SDL_surface.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SCRIPT_LINE_MAX        512
#define SCRIPT_FRAMES_PER_TASK 16     // emulated frames per host frame while waiting
#define SCRIPT_FRAME_US        16667  // one 60 Hz frame
#define SCRIPT_DEFAULT_UNTIL   600
#define SCRIPT_DEFAULT_FRAMES  120  // --screenshot without --frames

typedef enum {
    WAIT_NONE,
    WAIT_FRAMES,
    WAIT_ADDR,
} script_wait_t;

static FILE* script_file;
static const char* script_path = "<script>";
static char* script_text;  // for script_load_text()
static char* script_text_pos;
static int script_line_no;
static bool script_run;
static script_wait_t script_wait;
static uint32_t script_deadline;
static char script_last_line[SCRIPT_LINE_MAX];

// ---------------------------------------------------------------------------
// helpers

[[noreturn]] static void script_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] static void script_error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: ", script_path, script_line_no);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n  > %s\n", script_last_line);
    va_end(ap);
    fflush(stderr);
    fflush(stdout);
    exit(1);
}

static char* skip_ws(char* s) {
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

static char* script_word(char** p) {
    char* s = skip_ws(*p);
    if (!*s) {
        *p = s;
        return NULL;
    }
    char* w = s;
    while (*s && *s != ' ' && *s != '\t')
        s++;
    if (*s) *s++ = 0;
    *p = s;
    return w;
}

// "quoted string" or a bare word
static bool script_string(char** p, char* out, size_t outsz) {
    char* s = skip_ws(*p);
    if (*s == '"') {
        s++;
        size_t n = 0;
        while (*s && *s != '"') {
            if (n + 1 < outsz) out[n++] = *s;
            s++;
        }
        if (*s != '"') return false;
        s++;
        out[n] = 0;
        *p = s;
        return true;
    }
    char* w = script_word(&s);
    if (!w) return false;
    snprintf(out, outsz, "%s", w);
    *p = s;
    return true;
}

static bool script_number(char** p, long* out) {
    char* w = script_word(p);
    if (!w) return false;
    int base = 10;
    if (w[0] == '$') {
        w++;
        base = 16;
    }
    else if (w[0] == '0' && (w[1] == 'x' || w[1] == 'X')) {
        w += 2;
        base = 16;
    }
    char* end;
    long v = strtol(w, &end, base);
    if (end == w || *end) return false;
    *out = v;
    return true;
}

static bool script_number_opt(char** p, long* out, long def) {
    const char* s = skip_ws(*p);
    if (!*s || *s == '"') {
        *out = def;
        return true;
    }
    return script_number(p, out);
}

// ---------------------------------------------------------------------------
// display capture (SDL does the PNG encoding and the checksum)

// Wrap the CGIA framebuffer in a surface; `full` keeps the 2x DVI pixel repeat,
// otherwise it is scaled back down to the rasterized 384x240. The pixels are
// borrowed from the machine, so destroy the surface before the next frame.
static SDL_Surface* grab_display(x65_t* sys, bool full) {
    SDL_Surface* fb = SDL_CreateSurfaceFrom(
        CGIA_FRAMEBUFFER_WIDTH,
        CGIA_FRAMEBUFFER_HEIGHT,
        SDL_PIXELFORMAT_ABGR8888,
        sys->fb,
        CGIA_FRAMEBUFFER_WIDTH * 4);
    if (!fb || full) return fb;
    SDL_Surface* half =
        SDL_ScaleSurface(fb, CGIA_FRAMEBUFFER_WIDTH / 2, CGIA_FRAMEBUFFER_HEIGHT / 2, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(fb);
    return half;
}

static uint32_t display_crc(x65_t* sys) {
    return SDL_crc32(0, sys->fb, sizeof sys->fb);
}

// ---------------------------------------------------------------------------
// state printers

static void print_regs(x65_t* sys) {
    const w65816_t* c = &sys->cpu;
    printf(
        "CPU  PC=%02X:%04X A=%04X X=%04X Y=%04X S=%04X D=%04X DBR=%02X P=%02X [%c%c%c%c%c%c%c%c] E=%d %s frame=%u\n",
        c->PBR,
        c->PC,
        c->C,
        c->X,
        c->Y,
        c->S,
        c->D,
        c->DBR,
        c->P,
        (c->P & 0x80) ? 'N' : '.',
        (c->P & 0x40) ? 'V' : '.',
        (c->P & 0x20) ? 'M' : '.',
        (c->P & 0x10) ? 'X' : '.',
        (c->P & 0x08) ? 'D' : '.',
        (c->P & 0x04) ? 'I' : '.',
        (c->P & 0x02) ? 'Z' : '.',
        (c->P & 0x01) ? 'C' : '.',
        c->emulation,
        c->stopped == W65816_STOP_WAI ? "WAI" : (c->stopped ? "STP" : "run"),
        sys->cgia.frame_count);
    printf(
        "     ticks=%llu (frame ticks=%llu)\n",
        (unsigned long long)sys->hooks.tick_count,
        (unsigned long long)(sys->hooks.tick_count % X65_TICKS_PER_FRAME));
    fflush(stdout);
}

static void print_cgia(x65_t* sys) {
    printf("CGIA regs $FF00:");
    for (int i = 0; i < 0x80; i++) {
        if ((i & 15) == 0) printf("\n  %02X:", i);
        printf(" %02X", cgia_reg_read((uint8_t)i));
    }
    printf(
        "\n  raster=%u frame=%u int_mask=%02X vram_cache=[bank %u (want %u), bank %u (want %u)]\n",
        (unsigned)cgia_raster_line(),
        sys->cgia.frame_count,
        sys->cgia.int_mask,
        sys->cgia.vram_cache[0].bank,
        sys->cgia.vram_cache[0].wanted_bank,
        sys->cgia.vram_cache[1].bank,
        sys->cgia.vram_cache[1].wanted_bank);
    for (int p = 0; p < 4; p++) {
        printf(
            "  plane%d: memory_scan=%04X colour_scan=%04X backgr_scan=%04X chargen=%04X row_line=%u wait_vbl=%d sprites_upd=%d\n",
            p,
            sys->cgia.internal[p].memory_scan,
            sys->cgia.internal[p].colour_scan,
            sys->cgia.internal[p].backgr_scan,
            sys->cgia.internal[p].chargen_offset,
            sys->cgia.internal[p].row_line_count,
            sys->cgia.internal[p].wait_vbl,
            sys->cgia.internal[p].sprites_need_update);
    }
    fflush(stdout);
}

// Read a byte the way the CPU sees it (MMIO decoded) or the way the video chip
// sees it (raw RAM). The two differ under $FEC0-$FFFF, where a CPU access lands
// on a register while the CGIA's DMA still reads the memory cell underneath -
// so data a program tried to store there is invisible to `dump`.
static uint8_t script_read(x65_t* sys, uint32_t addr, bool raw) {
    return raw ? mem_ram_read(sys, addr) : mem_rd(sys, (uint8_t)(addr >> 16), (uint16_t)addr);
}

static void hex_dump(x65_t* sys, uint32_t addr, long count, bool raw) {
    for (long i = 0; i < count; i += 16) {
        printf("%06X:", (unsigned)(addr + i) & 0xFFFFFF);
        char ascii[17] = { 0 };
        for (long j = 0; j < 16; j++) {
            if (i + j < count) {
                uint32_t a = (addr + i + j) & 0xFFFFFF;
                uint8_t v = script_read(sys, a, raw);
                printf(" %02X", v);
                ascii[j] = isprint(v) ? (char)v : '.';
            }
            else {
                printf("   ");
            }
        }
        printf("  %s\n", ascii);
    }
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// commands

// joy [1|2] [up|down|left|right|a|b|c|d|none ...]
//
// The two DE-9 ports are independent, so each keeps its own mask and a call
// naming one port leaves the other alone.  Omitting the port means port 1,
// which is what every script wrote before ports existed.
static uint8_t joy_mask[2];

static void cmd_joy(x65_t* sys, char* p) {
    int port = 0;
    uint8_t mask = 0;
    bool first = true;
    char* w;
    while ((w = script_word(&p))) {
        if (first) {
            first = false;
            if (!strcmp(w, "1")) continue;
            if (!strcmp(w, "2")) {
                port = 1;
                continue;
            }
        }
        if (!strcasecmp(w, "up"))
            mask |= X65_JOYSTICK_UP;
        else if (!strcasecmp(w, "down"))
            mask |= X65_JOYSTICK_DOWN;
        else if (!strcasecmp(w, "left"))
            mask |= X65_JOYSTICK_LEFT;
        else if (!strcasecmp(w, "right"))
            mask |= X65_JOYSTICK_RIGHT;
        else if (!strcasecmp(w, "a"))
            mask |= X65_JOYSTICK_BTN;
        else if (!strcasecmp(w, "b"))
            mask |= X65_JOYSTICK_BTN2;
        // A DE-9 stick conventionally has one or two buttons, so there is no
        // standard name for the third and fourth.  X and Y mirror the HID
        // gamepad's naming and match the platform's controller example
        // (examples/src/io/controller.asm, JOY0_BUTTON_*); C and D are just
        // as defensible and stay accepted as aliases.
        else if (!strcasecmp(w, "x") || !strcasecmp(w, "c"))
            mask |= X65_JOYSTICK_BTN3;
        else if (!strcasecmp(w, "y") || !strcasecmp(w, "d"))
            mask |= X65_JOYSTICK_BTN4;
        else if (!strcasecmp(w, "none"))
            mask = 0;
        else
            script_error("joy: unknown line '%s'", w);
    }
    joy_mask[port] = mask;
    x65_joystick(sys, joy_mask[0], joy_mask[1]);
}

// pad <1..4> [button ...]  -- inject a USB HID gamepad report
//
// `joy` can only reach joystick 1, because the GPIO expander has two ports
// and cmd_joy fills one of them.  Everything with more than two players lives
// on the HID gamepads instead, which are normally fed only by real SDL
// devices, so this verb builds a report and pushes it in.
static const struct {
    const char* name;
    uint8_t reg;  // index into the ten-byte report
    uint8_t bit;
} pad_bits[] = {
    // dpad and feature byte
    { "up", 0, 0x01 },     { "down", 0, 0x02 },   { "left", 0, 0x04 },  { "right", 0, 0x08 },
    // The analog sticks, as the firmware's own digital encoding of them:
    // left stick in the low nibble, right stick in the high one, each in the
    // same up/down/left/right bit order as the dpad (pad_encode_stick).
    // Programs commonly merge this byte with the dpad so either works --
    // examples/src/io/controller.asm does exactly that -- so a script has to
    // be able to drive it.  The raw signed axes at $FFB4-$FFB7 are a separate
    // thing and are not injectable; nothing in the reference reads them.
    { "lup", 1, 0x01 },    { "ldown", 1, 0x02 },  { "lleft", 1, 0x04 }, { "lright", 1, 0x08 },
    { "rup", 1, 0x10 },    { "rdown", 1, 0x20 },  { "rleft", 1, 0x40 }, { "rright", 1, 0x80 },
    // button0: A B C X Y Z L R
    { "a", 2, 0x01 },      { "b", 2, 0x02 },      { "c", 2, 0x04 },     { "x", 2, 0x08 },
    { "y", 2, 0x10 },      { "z", 2, 0x20 },      { "l", 2, 0x40 },     { "r", 2, 0x80 },
    // button1: L2 R2 SELECT START HOME L3 R3
    { "l2", 3, 0x01 },     { "r2", 3, 0x02 },     { "select", 3, 0x04 },
    { "start", 3, 0x08 },  { "home", 3, 0x10 },   { "l3", 3, 0x20 },    { "r3", 3, 0x40 },
};

static void cmd_pad(char* p) {
    long index;
    if (!script_number(&p, &index) || index < 1 || index > RIA816_PAD_SLOTS)
        script_error("pad wants a controller index 1..%d", RIA816_PAD_SLOTS);

    uint8_t report[RIA816_PAD_REGS] = { 0 };
    char* w;
    while ((w = script_word(&p))) {
        if (!strcasecmp(w, "off")) {
            // Hand the slot back to whatever real device is plugged in.
            ria816_pad_release((uint8_t)index);
            return;
        }
        if (!strcasecmp(w, "none")) {
            memset(report, 0, sizeof report);
            continue;
        }
        size_t i = 0;
        for (; i < sizeof pad_bits / sizeof pad_bits[0]; ++i) {
            if (strcasecmp(w, pad_bits[i].name)) continue;
            report[pad_bits[i].reg] |= pad_bits[i].bit;
            break;
        }
        if (i == sizeof pad_bits / sizeof pad_bits[0]) script_error("pad: unknown button '%s'", w);
    }
    ria816_pad_inject((uint8_t)index, report);
}

// key [name|<code> ...]  -- hold this set of keys, USB HID usage ids
//
// Real key state only ever arrives from the host window, so a headless script
// cannot otherwise press a key.  Like `joy`, each call replaces the held set;
// `key` or `key none` releases everything.  Letters and digits map
// arithmetically, so only the awkward ones need naming.
static const struct {
    const char* name;
    uint8_t code;
} key_names[] = {
    { "up", 0x52 },      { "down", 0x51 },     { "left", 0x50 },   { "right", 0x4F },
    { "space", 0x2C },   { "enter", 0x28 },    { "escape", 0x29 }, { "tab", 0x2B },
    { "backspace", 0x2A }, { "minus", 0x2D },  { "equal", 0x2E },
    { "lctrl", 0xE0 },   { "lshift", 0xE1 },   { "lalt", 0xE2 },
    { "rctrl", 0xE4 },   { "rshift", 0xE5 },   { "ralt", 0xE6 },
    { "kp0", 0x62 },     { "kp1", 0x59 },      { "kp2", 0x5A },    { "kp3", 0x5B },
    { "kp4", 0x5C },     { "kp5", 0x5D },      { "kp6", 0x5E },    { "kp7", 0x5F },
    { "kp8", 0x60 },     { "kp9", 0x61 },      { "kpenter", 0x58 },
};

static bool script_key_code(const char* w, uint8_t* out) {
    if (w[0] && !w[1]) {
        const char c = (char)tolower((unsigned char)w[0]);
        if (c >= 'a' && c <= 'z') {
            *out = (uint8_t)(0x04 + (c - 'a'));  // HID_KEY_A
            return true;
        }
        if (c >= '1' && c <= '9') {
            *out = (uint8_t)(0x1E + (c - '1'));  // HID_KEY_1
            return true;
        }
        if (c == '0') {
            *out = 0x27;  // HID_KEY_0
            return true;
        }
    }
    for (size_t i = 0; i < sizeof key_names / sizeof key_names[0]; ++i) {
        if (!strcasecmp(w, key_names[i].name)) {
            *out = key_names[i].code;
            return true;
        }
    }
    return false;
}

static void cmd_key(char* p) {
    ria816_keys_clear();
    char* w;
    while ((w = script_word(&p))) {
        if (!strcasecmp(w, "none")) {
            ria816_keys_clear();
            continue;
        }
        uint8_t code;
        if (script_key_code(w, &code)) {
            ria816_key_set(code);
            continue;
        }
        // Anything else has to be a numeric usage id.
        char* q = w;
        long value;
        if (!script_number(&q, &value) || value < 0 || value > 255)
            script_error("key: unknown key '%s'", w);
        ria816_key_set((uint8_t)value);
    }
}

static void script_command(x65_t* sys, const char* line) {
    char buf[SCRIPT_LINE_MAX];
    snprintf(buf, sizeof buf, "%s", line);
    char* end = buf + strlen(buf);
    while (end > buf && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
        *--end = 0;
    snprintf(script_last_line, sizeof script_last_line, "%s", buf);

    char* p = buf;
    char* cmd = script_word(&p);
    if (!cmd || cmd[0] == '#') return;

    if (!strcasecmp(cmd, "run")) {
        long frames;
        if (!script_number_opt(&p, &frames, 1) || frames < 0) script_error("run wants a frame count");
        script_wait = WAIT_FRAMES;
        script_deadline = sys->cgia.frame_count + (uint32_t)frames;
        return;
    }
    if (!strcasecmp(cmd, "until")) {
        long addr, frames;
        if (!script_number(&p, &addr)) script_error("until wants an address");
        if (!script_number_opt(&p, &frames, SCRIPT_DEFAULT_UNTIL)) script_error("until: bad frame budget");
        sys->hooks.break_addr = (uint32_t)addr & 0xFFFFFF;
        sys->hooks.break_hit = false;
        script_wait = WAIT_ADDR;
        script_deadline = sys->cgia.frame_count + (uint32_t)frames;
        return;
    }
    if (!strcasecmp(cmd, "joy")) {
        cmd_joy(sys, p);
        return;
    }
    if (!strcasecmp(cmd, "pad")) {
        cmd_pad(p);
        return;
    }
    if (!strcasecmp(cmd, "key")) {
        cmd_key(p);
        return;
    }
    if (!strcasecmp(cmd, "shot")) {
        char path[256];
        if (!script_string(&p, path, sizeof path)) script_error("shot wants a file name");
        char* w = script_word(&p);
        SDL_Surface* shot = grab_display(sys, w && !strcasecmp(w, "full"));
        if (!shot) script_error("shot: %s", SDL_GetError());
        const bool ok = SDL_SavePNG(shot, path);
        const int wd = shot->w, ht = shot->h;
        SDL_DestroySurface(shot);
        if (!ok) script_error("shot: cannot write %s (%s)", path, SDL_GetError());
        printf("shot %s (%dx%d) frame=%u\n", path, wd, ht, sys->cgia.frame_count);
        fflush(stdout);
        return;
    }
    if (!strcasecmp(cmd, "crc")) {
        printf("crc %08X frame=%u\n", display_crc(sys), sys->cgia.frame_count);
        fflush(stdout);
        return;
    }
    if (!strcasecmp(cmd, "expect-crc")) {
        long want;
        if (!script_number(&p, &want)) script_error("expect-crc wants a hash");
        uint32_t got = display_crc(sys);
        if (got != (uint32_t)want) script_error("display crc %08X, expected %08lX", got, want);
        return;
    }
    if (!strcasecmp(cmd, "dump") || !strcasecmp(cmd, "vdump")) {
        const bool raw = cmd[0] == 'v' || cmd[0] == 'V';
        long addr, count;
        if (!script_number(&p, &addr)) script_error("%s wants an address", cmd);
        if (!script_number_opt(&p, &count, 64)) script_error("%s: bad count", cmd);
        if (!*skip_ws(p)) {
            hex_dump(sys, (uint32_t)addr, count, raw);
            return;
        }
        char path[256];
        if (!script_string(&p, path, sizeof path)) script_error("%s: bad file name", cmd);
        FILE* f = fopen(path, "wb");
        if (!f) script_error("%s: cannot write %s", cmd, path);
        for (long i = 0; i < count; i++) {
            uint32_t a = ((uint32_t)addr + i) & 0xFFFFFF;
            fputc(script_read(sys, a, raw), f);
        }
        fclose(f);
        printf("%s %06lX +%ld -> %s\n", cmd, addr, count, path);
        fflush(stdout);
        return;
    }
    if (!strcasecmp(cmd, "peek") || !strcasecmp(cmd, "poke") || !strcasecmp(cmd, "vpeek")
        || !strcasecmp(cmd, "vpoke")) {
        const bool raw = cmd[0] == 'v' || cmd[0] == 'V';
        const bool write = !strcasecmp(cmd, raw ? "vpoke" : "poke");
        long addr, v;
        if (!script_number(&p, &addr)) script_error("%s wants an address", cmd);
        int n = 0;
        while (script_number(&p, &v)) {
            uint32_t a = ((uint32_t)addr + n) & 0xFFFFFF;
            if (write) {
                if (raw)
                    // straight into the memory cell, and into the CGIA's copy of it,
                    // so it takes effect on the very next scanline
                    mem_ram_write(sys, a, (uint8_t)v);
                else
                    mem_wr(sys, (uint8_t)(a >> 16), (uint16_t)a, (uint8_t)v);
            }
            else {
                uint8_t got = script_read(sys, a, raw);
                if (got != (uint8_t)v) script_error("$%06X is %02X, expected %02lX", a, got, v & 0xFF);
            }
            n++;
        }
        if (!n) script_error("%s wants at least one byte", cmd);
        return;
    }
    if (!strcasecmp(cmd, "regs")) {
        print_regs(sys);
        return;
    }
    if (!strcasecmp(cmd, "cgia")) {
        print_cgia(sys);
        return;
    }
    if (!strcasecmp(cmd, "trace")) {
        long n;
        if (!script_number_opt(&p, &n, 32) || n < 0) script_error("trace wants a count");
        sys->hooks.trace_remaining = (uint32_t)n;
        return;
    }
    if (!strcasecmp(cmd, "echo")) {
        char text[SCRIPT_LINE_MAX];
        if (!script_string(&p, text, sizeof text)) text[0] = 0;
        printf("%s\n", text);
        fflush(stdout);
        return;
    }
    if (!strcasecmp(cmd, "exit")) {
        long code;
        if (!script_number_opt(&p, &code, 0)) script_error("exit: bad code");
        fflush(stdout);
        fflush(stderr);
        exit((int)code);
    }
    script_error("unknown command '%s'", cmd);
}

// ---------------------------------------------------------------------------
// driver

static bool next_line(char* buf, size_t size) {
    if (script_file) {
        if (!fgets(buf, (int)size, script_file)) return false;
        return true;
    }
    if (script_text_pos && *script_text_pos) {
        char* nl = strchr(script_text_pos, '\n');
        size_t n = nl ? (size_t)(nl - script_text_pos) : strlen(script_text_pos);
        if (n >= size) n = size - 1;
        memcpy(buf, script_text_pos, n);
        buf[n] = 0;
        script_text_pos += n + (nl ? 1 : 0);
        return true;
    }
    return false;
}

// true when the pending wait is over
static bool settle(x65_t* sys) {
    const bool expired = (int32_t)(sys->cgia.frame_count - script_deadline) >= 0;
    switch (script_wait) {
        case WAIT_NONE: return true;
        case WAIT_FRAMES:
            if (expired) script_wait = WAIT_NONE;
            return expired;
        case WAIT_ADDR:
            if (sys->hooks.break_hit) {
                sys->hooks.break_hit = false;
                sys->hooks.break_addr = X65_NO_BREAK_ADDR;
                script_wait = WAIT_NONE;
                return true;
            }
            if (expired) {
                sys->hooks.break_addr = X65_NO_BREAK_ADDR;
                script_error("until: address not reached in time (PC=%02X:%04X)", sys->cpu.PBR, sys->cpu.PC);
            }
            return false;
    }
    return true;
}

void script_task(x65_t* sys) {
    if (!script_run) return;
    for (int i = 0; i < SCRIPT_FRAMES_PER_TASK && script_run; i++) {
        if (!settle(sys)) {
            x65_exec(sys, SCRIPT_FRAME_US);
            continue;
        }
        // run commands until one has to wait
        char line[SCRIPT_LINE_MAX];
        while (script_wait == WAIT_NONE) {
            if (!next_line(line, sizeof line)) {
                script_run = false;
                LOG_INFO("script finished");
                break;
            }
            script_line_no++;
            script_command(sys, line);
        }
    }
}

static void script_begin(const char* path) {
    script_path = path;
    script_line_no = 0;
    script_run = true;
    script_wait = WAIT_NONE;
}

bool script_load(const char* path) {
    const bool is_stdin = !strcmp(path, "-");
    script_file = is_stdin ? stdin : fopen(path, "r");
    if (!script_file) {
        LOG_ERROR("cannot open script %s", path);
        return false;
    }
    script_begin(is_stdin ? "<stdin>" : path);
    return true;
}

static void script_load_text(const char* text) {
    free(script_text);
    script_text = strdup(text);
    script_text_pos = script_text;
    script_file = NULL;
    script_begin("<args>");
}

void script_screenshot(const char* path, const char* frames) {
    unsigned long n = SCRIPT_DEFAULT_FRAMES;
    if (frames) {
        char* end;
        n = strtoul(frames, &end, 10);
        if (end == frames || *end) {
            LOG_ERROR("--frames wants a number, got '%s'", frames);
            exit(EXIT_FAILURE);
        }
    }
    if (strchr(path, '"')) {
        LOG_ERROR("--screenshot file name must not contain a quote: %s", path);
        exit(EXIT_FAILURE);
    }
    char text[512];
    snprintf(text, sizeof text, "run %lu\nshot \"%s\"\nexit 0\n", n, path);
    script_load_text(text);
}

bool script_running(void) {
    return script_run;
}
