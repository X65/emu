/**
 * Runner for the SingleStepTests/65816 suite.
 *
 * https://github.com/SingleStepTests/65816 provides, for every opcode, 10000
 * single-instruction tests in native mode and 10000 in emulation mode. Each
 * test carries the full processor and memory state before and after the
 * instruction, plus a cycle-by-cycle breakdown of bus activity - which lines
 * up one-to-one with the pin mask the w65816 core exposes.
 *
 * The data set is ~3 GB of JSON and is not vendored. Fetch it (all of it, or
 * just the opcodes you care about) with tools/fetch-sst65816.sh, then:
 *
 *     build/src/tests/sst65816 sst65816/v1
 *     build/src/tests/sst65816 -o 3d -o 48 -v sst65816/v1
 *
 * MLB is not compared: the core has no memory-lock output pin.
 */

#define CHIPS_IMPL
#include "chips/w65c816s.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr size_t MEM_SIZE = 1 << 24;  // the tests assume a flat 16 MB address space

// ---------------------------------------------------------------- JSON reader

/* Strict single-pass reader for the fixed shape of the test files. Everything
   unexpected throws, so a change in the data format can never be mis-parsed
   into a silently passing test. */

class json_error: public std::runtime_error
{
  public:
    explicit json_error(const std::string& what) : std::runtime_error(what) {}
};

class json_reader
{
  public:
    json_reader(const char* data, size_t size, std::string name) :
        _begin(data),
        _p(data),
        _end(data + size),
        _name(std::move(name)) {}

    void ws() {
        while (_p < _end && (*_p == ' ' || *_p == '\t' || *_p == '\n' || *_p == '\r')) {
            ++_p;
        }
    }
    bool at(char c) {
        ws();
        return _p < _end && *_p == c;
    }
    bool take(char c) {
        if (at(c)) {
            ++_p;
            return true;
        }
        return false;
    }
    void expect(char c) {
        if (!take(c)) {
            fail(std::string("'") + c + "'");
        }
    }
    std::string_view str() {
        ws();
        if (_p >= _end || *_p != '"') {
            fail("string");
        }
        ++_p;
        const char* const s = _p;
        while (_p < _end && *_p != '"') {
            if (*_p == '\\') {
                fail("string without escapes");
            }
            ++_p;
        }
        if (_p >= _end) {
            fail("closing quote");
        }
        const std::string_view v(s, (size_t)(_p - s));
        ++_p;
        return v;
    }

    uint32_t uint32(uint32_t limit) {
        ws();
        if (_p >= _end || *_p < '0' || *_p > '9') {
            fail("unsigned integer");
        }
        uint64_t v = 0;
        while (_p < _end && *_p >= '0' && *_p <= '9') {
            v = v * 10 + (uint64_t)(*_p++ - '0');
            if (v > limit) {
                fail("integer within range");
            }
        }
        return (uint32_t)v;
    }

    bool take_null() {
        ws();
        if ((size_t)(_end - _p) >= 4 && 0 == memcmp(_p, "null", 4)) {
            _p += 4;
            return true;
        }
        return false;
    }

    [[noreturn]] void fail(const std::string& what) const {
        throw json_error(_name + ": byte " + std::to_string((size_t)(_p - _begin)) + ": expected " + what);
    }

  private:
    const char* _begin;
    const char* _p;
    const char* _end;
    std::string _name;
};

// ------------------------------------------------------------- test structure

struct mem_val_t {
    uint32_t addr;
    uint8_t value;
};

struct cycle_t {
    int64_t addr;  // -1 while the bus is floating, as it is after STP/WAI
    int data;      // -1 for the JSON null of an unqualified bus cycle
    char flags[9];

    bool is_fetch() const {
        return flags[0] == 'd' && flags[1] == 'p';
    }
    /* STP and WAI park the processor with the bus floating, which the suite
       records as an addressless cycle with even RWB inactive. The core keeps
       driving the last address instead, so all that can be compared is that
       it too has stopped qualifying the bus. */
    bool is_halted() const {
        return addr < 0 && flags[3] == '-';
    }
};

struct state_t {
    uint16_t pc, s, a, x, y, d;
    uint8_t p, dbr, pbr, e;
    std::vector<mem_val_t> ram;
};

struct test_t {
    std::string name;
    state_t initial;
    state_t expected;
    std::vector<cycle_t> cycles;
};

void parse_ram(json_reader& r, std::vector<mem_val_t>& ram) {
    ram.clear();
    r.expect('[');
    if (r.take(']')) {
        return;
    }
    do {
        r.expect('[');
        mem_val_t mv;
        mv.addr = r.uint32(0xFFFFFF);
        r.expect(',');
        mv.value = (uint8_t)r.uint32(0xFF);
        r.expect(']');
        ram.push_back(mv);
    } while (r.take(','));
    r.expect(']');
}

void parse_state(json_reader& r, state_t& st) {
    unsigned seen = 0;
    r.expect('{');
    do {
        const std::string_view k = r.str();
        r.expect(':');
        // clang-format off
        if      (k == "pc")  { st.pc  = (uint16_t)r.uint32(0xFFFF); seen |= 1u << 0; }
        else if (k == "s")   { st.s   = (uint16_t)r.uint32(0xFFFF); seen |= 1u << 1; }
        else if (k == "p")   { st.p   = (uint8_t) r.uint32(0xFF);   seen |= 1u << 2; }
        else if (k == "a")   { st.a   = (uint16_t)r.uint32(0xFFFF); seen |= 1u << 3; }
        else if (k == "x")   { st.x   = (uint16_t)r.uint32(0xFFFF); seen |= 1u << 4; }
        else if (k == "y")   { st.y   = (uint16_t)r.uint32(0xFFFF); seen |= 1u << 5; }
        else if (k == "dbr") { st.dbr = (uint8_t) r.uint32(0xFF);   seen |= 1u << 6; }
        else if (k == "d")   { st.d   = (uint16_t)r.uint32(0xFFFF); seen |= 1u << 7; }
        else if (k == "pbr") { st.pbr = (uint8_t) r.uint32(0xFF);   seen |= 1u << 8; }
        else if (k == "e")   { st.e   = (uint8_t) r.uint32(1);      seen |= 1u << 9; }
        else if (k == "ram") { parse_ram(r, st.ram);                seen |= 1u << 10; }
        else { r.fail("known state key, got \"" + std::string(k) + "\""); }
        // clang-format on
    } while (r.take(','));
    r.expect('}');
    if (seen != (1u << 11) - 1) {
        r.fail("a complete processor state");
    }
}

void parse_cycles(json_reader& r, std::vector<cycle_t>& cycles) {
    cycles.clear();
    r.expect('[');
    if (r.take(']')) {
        return;
    }
    do {
        r.expect('[');
        cycle_t c;
        c.addr = r.take_null() ? -1 : (int64_t)r.uint32(0xFFFFFF);
        r.expect(',');
        c.data = r.take_null() ? -1 : (int)r.uint32(0xFF);
        r.expect(',');
        const std::string_view f = r.str();
        if (f.size() != 8) {
            r.fail("8-character bus state");
        }
        memcpy(c.flags, f.data(), 8);
        c.flags[8] = '\0';
        r.expect(']');
        cycles.push_back(c);
    } while (r.take(','));
    r.expect(']');
}

void parse_test(json_reader& r, test_t& t) {
    unsigned seen = 0;
    r.expect('{');
    do {
        const std::string_view k = r.str();
        r.expect(':');
        // clang-format off
        if      (k == "name")    { t.name.assign(r.str());      seen |= 1u << 0; }
        else if (k == "initial") { parse_state(r, t.initial);   seen |= 1u << 1; }
        else if (k == "final")   { parse_state(r, t.expected);  seen |= 1u << 2; }
        else if (k == "cycles")  { parse_cycles(r, t.cycles);   seen |= 1u << 3; }
        else { r.fail("known test key, got \"" + std::string(k) + "\""); }
        // clang-format on
    } while (r.take(','));
    r.expect('}');
    if (seen != 0xF) {
        r.fail("a complete test");
    }
}

// ---------------------------------------------------------------- the machine

struct options_t {
    std::vector<int> opcodes;  // empty: all
    bool native = true;
    bool emulation = true;
    uint64_t limit = 0;  // 0: no limit
    unsigned show = 8;   // failing tests to describe per file
    unsigned jobs = 1;
};

struct failure_t {
    std::string category;
    std::string detail;
};

struct tally_t {
    uint64_t passed = 0;
    uint64_t failed = 0;
    std::map<std::string, uint64_t> categories;

    void merge(const tally_t& other) {
        passed += other.passed;
        failed += other.failed;
        for (const auto& [category, count] : other.categories) {
            categories[category] += count;
        }
    }
};

/* Owns the 16 MB address space and the CPU, so that several files can be
   replayed in parallel. */
class machine_t
{
  public:
    machine_t() : _mem(MEM_SIZE, 0) {
        w65816_desc_t desc = {};
        uint64_t pins = w65816_init(&_pristine, &desc);
        // let the reset sequence run to completion; it reads the reset vector
        // out of still-zeroed memory and suppresses its stack writes, so the
        // resulting state is a clean instruction boundary
        for (int i = 0; i < 7; i++) {
            W65816_SET_DATA(pins, 0);
            pins = w65816_tick(&_pristine, pins);
        }
    }

    /* Replay one test, appending every divergence to `out`. */
    void run(const test_t& t, std::vector<failure_t>& out) {
        _cpu = _pristine;
        _dirty.clear();

        for (const mem_val_t& mv : t.initial.ram) {
            _mem[mv.addr] = mv.value;
            _dirty.push_back(mv.addr);
        }

        w65816_set_e(&_cpu, t.initial.e);
        w65816_set_p(&_cpu, t.initial.p);
        w65816_set_c(&_cpu, t.initial.a);
        w65816_set_x(&_cpu, t.initial.x);
        w65816_set_y(&_cpu, t.initial.y);
        w65816_set_s(&_cpu, t.initial.s);
        w65816_set_d(&_cpu, t.initial.d);
        w65816_set_pc(&_cpu, t.initial.pc);
        w65816_set_pb(&_cpu, t.initial.pbr);
        w65816_set_db(&_cpu, t.initial.dbr);

        // enter at the opcode fetch of the instruction under test
        uint64_t pins = W65816_RW | W65816_VDA | W65816_VPA;
        W65816_SET_ADDR(pins, ((uint32_t)t.initial.pbr << 16) | t.initial.pc);
        _cpu.PINS = pins;

        bool desynced = false;
        for (size_t i = 0; i < t.cycles.size(); i++) {
            const cycle_t& want = t.cycles[i];

            char flags[9];
            flags[0] = (pins & W65816_VDA) ? 'd' : '-';
            flags[1] = (pins & W65816_VPA) ? 'p' : '-';
            flags[2] = (pins & W65816_VPB) ? 'v' : '-';
            flags[3] = (pins & W65816_RW) ? 'r' : 'w';
            flags[4] = _cpu.emulation ? 'e' : '-';
            flags[5] = (_cpu.emulation || (_cpu.P & W65816_MF)) ? 'm' : '-';
            flags[6] = (_cpu.emulation || (_cpu.P & W65816_XF)) ? 'x' : '-';
            flags[7] = '-';  // MLB is not emulated, so it is never compared
            flags[8] = '\0';

            const uint32_t addr = W65816_GET_ADDR(pins);
            const bool qualified = 0 != (pins & (W65816_VDA | W65816_VPA | W65816_VPB));
            const bool read = 0 != (pins & W65816_RW);

            // the generating environment only enables RAM on a qualified cycle
            int data = -1;
            if (qualified) {
                if (read) {
                    data = _mem[addr];
                    W65816_SET_DATA(pins, (uint8_t)data);
                }
                else {
                    data = W65816_GET_DATA(pins);
                    _mem[addr] = (uint8_t)data;
                    _dirty.push_back(addr);
                }
            }

            // an opcode fetch where none is expected (or the reverse) means
            // the instruction ran long or short; every later cycle would then
            // be off by a fixed shift, so stop rather than report the cascade
            if (want.is_fetch() != (0 != W65816_IS_FETCH(pins))) {
                out.push_back({ "cycle-sync", cycle_detail(i, want, addr, data, flags) });
                desynced = true;
                break;
            }
            if (want.is_halted()) {
                if (qualified) {
                    out.push_back({ "cycle-qual", cycle_detail(i, want, addr, data, flags) });
                }
            }
            else if (want.addr >= 0 && addr != (uint32_t)want.addr) {
                out.push_back({ "cycle-addr", cycle_detail(i, want, addr, data, flags) });
            }
            else if (0 != memcmp(flags, want.flags, 4)) {
                // VDA/VPA/VPB/RWB: pins the core actually drives
                out.push_back({ "cycle-qual", cycle_detail(i, want, addr, data, flags) });
            }
            else if (0 != memcmp(flags + 4, want.flags + 4, 3)) {
                // E/M/X: derived from the status register, since the core has
                // no MX output pin - a mismatch dates when a flag change lands
                out.push_back({ "cycle-mx", cycle_detail(i, want, addr, data, flags) });
            }
            else if (want.data >= 0 && data >= 0 && data != want.data) {
                out.push_back({ "cycle-data", cycle_detail(i, want, addr, data, flags) });
            }

            pins = w65816_tick(&_cpu, pins);
        }

        // once the cycle streams have diverged the instruction never ran to
        // the expected point, so comparing the end state only adds noise
        if (desynced) {
            for (const uint32_t addr : _dirty) {
                _mem[addr] = 0;
            }
            return;
        }

        // clang-format off
        cmp(out, "pc",  t.expected.pc,  w65816_pc(&_cpu));
        cmp(out, "s",   t.expected.s,   w65816_s(&_cpu));
        cmp(out, "p",   t.expected.p,   w65816_p(&_cpu));
        cmp(out, "a",   t.expected.a,   w65816_c(&_cpu));
        cmp(out, "x",   t.expected.x,   w65816_x(&_cpu));
        cmp(out, "y",   t.expected.y,   w65816_y(&_cpu));
        cmp(out, "d",   t.expected.d,   w65816_d(&_cpu));
        cmp(out, "dbr", t.expected.dbr, w65816_db(&_cpu));
        cmp(out, "pbr", t.expected.pbr, w65816_pb(&_cpu));
        cmp(out, "e",   t.expected.e,   (unsigned)w65816_e(&_cpu));
        // clang-format on

        for (const mem_val_t& mv : t.expected.ram) {
            if (_mem[mv.addr] != mv.value) {
                char buf[64];
                snprintf(buf, sizeof(buf), "[%06X] is %02X, expected %02X", mv.addr, _mem[mv.addr], mv.value);
                out.push_back({ "ram", buf });
            }
        }

        for (const uint32_t addr : _dirty) {
            _mem[addr] = 0;
        }
    }

  private:
    static void cmp(std::vector<failure_t>& out, const char* what, unsigned want, unsigned got) {
        if (want != got) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s is %04X, expected %04X", what, got, want);
            out.push_back({ what, buf });
        }
    }

    static std::string cycle_detail(size_t i, const cycle_t& want, uint32_t addr, int data, const char* flags) {
        char buf[128];
        char got_addr[8], want_addr[8], got_data[8], want_data[8];
        snprintf(got_addr, sizeof(got_addr), "%06X", addr);
        snprintf(want_addr, sizeof(want_addr), want.addr < 0 ? "------" : "%06X", (unsigned)want.addr);
        snprintf(got_data, sizeof(got_data), data < 0 ? "--" : "%02X", data);
        snprintf(want_data, sizeof(want_data), want.data < 0 ? "--" : "%02X", want.data);
        snprintf(
            buf,
            sizeof(buf),
            "cycle %zu: %s %s %s, expected %s %s %s",
            i,
            got_addr,
            got_data,
            flags,
            want_addr,
            want_data,
            want.flags);
        return buf;
    }

    std::vector<uint8_t> _mem;
    std::vector<uint32_t> _dirty;
    w65816_t _pristine;
    w65816_t _cpu;
};

// ------------------------------------------------------------------- file run

std::mutex g_print;

std::string read_file(const std::filesystem::path& path) {
    // not path.c_str(): that is wchar_t on Windows
    const std::string name = path.string();
    FILE* f = fopen(name.c_str(), "rb");
    if (!f) {
        throw json_error(name + ": " + strerror(errno));
    }
    std::string data;
    char buf[1 << 16];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        data.append(buf, n);
    }
    const bool failed = ferror(f);
    fclose(f);
    if (failed) {
        throw json_error(name + ": read error");
    }
    return data;
}

tally_t run_file(const std::filesystem::path& path, const options_t& opt, machine_t& machine) {
    const std::string data = read_file(path);
    const std::string label = path.filename().string();
    json_reader r(data.data(), data.size(), label);

    tally_t tally;
    test_t t;
    std::vector<failure_t> failures;
    std::string report;
    unsigned shown = 0;
    bool truncated = false;

    r.expect('[');
    if (!r.take(']')) {
        do {
            parse_test(r, t);
            failures.clear();
            machine.run(t, failures);
            if (failures.empty()) {
                tally.passed++;
            }
            else {
                tally.failed++;
                for (const failure_t& f : failures) {
                    tally.categories[f.category]++;
                }
                if (shown++ < opt.show) {
                    report += "  " + label + " \"" + t.name + "\"\n";
                    for (const failure_t& f : failures) {
                        report += "    " + f.detail + "\n";
                    }
                }
            }
            if (opt.limit && tally.passed + tally.failed >= opt.limit) {
                truncated = true;
                break;
            }
        } while (r.take(','));
        if (!truncated) {
            r.expect(']');
        }
    }

    std::string summary;
    for (const auto& [category, count] : tally.categories) {
        summary += " " + category + "=" + std::to_string(count);
    }

    const std::lock_guard<std::mutex> lock(g_print);
    printf(
        "%-12s %6llu/%-6llu ok%s\n",
        label.c_str(),
        (unsigned long long)tally.passed,
        (unsigned long long)(tally.passed + tally.failed),
        summary.c_str());
    fputs(report.c_str(), stdout);
    fflush(stdout);
    return tally;
}

// ----------------------------------------------------------------- collection

bool wanted(const std::filesystem::path& path, const options_t& opt) {
    // files are named "<2 hex digits>.<e|n>.json"
    const std::string name = path.filename().string();
    if (name.size() != 9 || name[2] != '.' || name.compare(4, 5, ".json") != 0) {
        return false;
    }
    if (name[3] == 'e' ? !opt.emulation : name[3] == 'n' ? !opt.native : true) {
        return false;
    }
    char* stop = nullptr;
    const long opcode = strtol(name.substr(0, 2).c_str(), &stop, 16);
    if (!stop || *stop != '\0') {
        return false;
    }
    return opt.opcodes.empty() || std::find(opt.opcodes.begin(), opt.opcodes.end(), (int)opcode) != opt.opcodes.end();
}

std::vector<std::filesystem::path> collect(const std::vector<std::string>& paths, const options_t& opt) {
    std::vector<std::filesystem::path> files;
    for (const std::string& arg : paths) {
        const std::filesystem::path p(arg);
        if (std::filesystem::is_directory(p)) {
            for (const auto& entry : std::filesystem::directory_iterator(p)) {
                if (entry.is_regular_file() && wanted(entry.path(), opt)) {
                    files.push_back(entry.path());
                }
            }
        }
        else {
            files.push_back(p);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

void usage(const char* argv0) {
    printf(
        "Usage: %s [OPTION]... PATH...\n"
        "\n"
        "Replay SingleStepTests/65816 JSON tests against the w65816 core.\n"
        "PATH is a directory of <opcode>.<e|n>.json files, or such files themselves.\n"
        "\n"
        "  -o, --opcode HEX   only this opcode (repeatable)\n"
        "  -e, --emulation    only emulation-mode files\n"
        "  -n, --native       only native-mode files\n"
        "  -l, --limit N      stop after N tests per file\n"
        "  -s, --show N       describe at most N failing tests per file (default 8)\n"
        "  -j, --jobs N       replay N files in parallel (default 1)\n"
        "  -v, --verbose      describe every failing test\n"
        "  -h, --help         this message\n",
        argv0);
}

}  // namespace

int main(int argc, char** argv) {
    options_t opt;
    std::vector<std::string> paths;

    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        const char* value = (i + 1 < argc) ? argv[i + 1] : nullptr;
        // clang-format off
        if      (arg == "-h" || arg == "--help")      { usage(argv[0]); return 0; }
        else if (arg == "-v" || arg == "--verbose")   { opt.show = UINT_MAX; }
        else if (arg == "-e" || arg == "--emulation") { opt.native = false; }
        else if (arg == "-n" || arg == "--native")    { opt.emulation = false; }
        else if (arg == "-o" || arg == "--opcode")    { if (!value) { usage(argv[0]); return 2; } opt.opcodes.push_back((int)strtol(argv[++i], nullptr, 16)); }
        else if (arg == "-l" || arg == "--limit")     { if (!value) { usage(argv[0]); return 2; } opt.limit = strtoull(argv[++i], nullptr, 10); }
        else if (arg == "-s" || arg == "--show")      { if (!value) { usage(argv[0]); return 2; } opt.show = (unsigned)strtoul(argv[++i], nullptr, 10); }
        else if (arg == "-j" || arg == "--jobs")      { if (!value) { usage(argv[0]); return 2; } opt.jobs = (unsigned)strtoul(argv[++i], nullptr, 10); }
        else if (!arg.empty() && arg[0] == '-')       { fprintf(stderr, "unknown option: %s\n", arg.c_str()); return 2; }
        else                                          { paths.push_back(arg); }
        // clang-format on
    }

    if (paths.empty()) {
        usage(argv[0]);
        return 2;
    }

    std::vector<std::filesystem::path> files;
    try {
        files = collect(paths, opt);
    }
    catch (const std::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 2;
    }
    if (files.empty()) {
        fprintf(stderr, "no matching test files found\n");
        return 2;
    }

    const unsigned jobs = std::max(1u, std::min(opt.jobs, (unsigned)files.size()));
    std::atomic<size_t> next(0);
    std::atomic<bool> broken(false);
    std::vector<tally_t> tallies(jobs);

    const auto worker = [&](unsigned slot) {
        machine_t machine;
        for (;;) {
            const size_t i = next.fetch_add(1);
            if (i >= files.size()) {
                return;
            }
            try {
                tallies[slot].merge(run_file(files[i], opt, machine));
            }
            catch (const std::exception& e) {
                const std::lock_guard<std::mutex> lock(g_print);
                fprintf(stderr, "%s\n", e.what());
                broken = true;
            }
        }
    };

    if (jobs == 1) {
        worker(0);
    }
    else {
        std::vector<std::thread> threads;
        for (unsigned j = 0; j < jobs; j++) {
            threads.emplace_back(worker, j);
        }
        for (std::thread& t : threads) {
            t.join();
        }
    }

    tally_t total;
    for (const tally_t& t : tallies) {
        total.merge(t);
    }

    printf(
        "\n%zu files, %llu tests, %llu passed, %llu failed\n",
        files.size(),
        (unsigned long long)(total.passed + total.failed),
        (unsigned long long)total.passed,
        (unsigned long long)total.failed);
    if (!total.categories.empty()) {
        printf("divergences by kind:\n");
        for (const auto& [category, count] : total.categories) {
            printf("  %-12s %llu\n", category.c_str(), (unsigned long long)count);
        }
    }

    return (broken || total.failed) ? 1 : 0;
}
