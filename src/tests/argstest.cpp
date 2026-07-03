#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <initializer_list>
#include <vector>

extern "C" {
#include "args.h"
}

// --- Stubs for symbols args.c expects from the rest of the app -------------
// The parser records the label file it is asked to load so tests can assert on
// the -l / --labels path without pulling in the real (GUI-bound) loader.
extern "C" {
const char* app_name = "emu";
char program_version[] = "emu test";

int g_labels_calls = 0;
const char* g_last_label_file = nullptr;
void app_load_labels(const char* file, bool clear) {
    (void)clear;
    g_labels_calls++;
    g_last_label_file = file;
}
}

// Parse the given arguments (argv[0] is synthesised) into a fresh, default
// arguments struct and return the parse status.
static args_status_t parse(std::initializer_list<const char*> args, struct arguments& out,
                           const char** errmsg = nullptr) {
    std::vector<char*> argv;
    argv.push_back(strdup("emu"));  // program name
    for (const char* a : args)
        argv.push_back(strdup(a));
    argv.push_back(nullptr);

    out = args_defaults();
    return args_parse_argv(argv.data(), &out, errmsg);
}

TEST_CASE("defaults with no arguments") {
    struct arguments a;
    CHECK(parse({}, a) == ARGS_OK);
    CHECK(a.rom == nullptr);
    CHECK(std::strcmp(a.output_file, "-") == 0);
    CHECK_FALSE(a.silent);
    CHECK_FALSE(a.verbose);
    CHECK_FALSE(a.fullscreen);
    CHECK_FALSE(a.crt);
    CHECK(a.crt_values == nullptr);
    CHECK(a.joystick == nullptr);
    CHECK(a.break_opcode == nullptr);
}

TEST_CASE("long boolean flags") {
    struct arguments a;
    CHECK(parse({ "--verbose", "--fullscreen", "--zero-mem", "--disable-gui", "--dap",
                  "--disable-speaker-icon" },
                a)
          == ARGS_OK);
    CHECK(a.verbose);
    CHECK(a.fullscreen);
    CHECK(a.zeromem);
    CHECK(a.disable_gui);
    CHECK(a.dap);
    CHECK(a.disable_speaker_icon);
}

TEST_CASE("short flags, including bundled") {
    struct arguments a;
    CHECK(parse({ "-vzf" }, a) == ARGS_OK);
    CHECK(a.verbose);
    CHECK(a.zeromem);
    CHECK(a.fullscreen);
}

TEST_CASE("quiet/silent aliases all set silent") {
    struct arguments a;
    CHECK(parse({ "-q" }, a) == ARGS_OK);
    CHECK(a.silent);
    CHECK(parse({ "-s" }, a) == ARGS_OK);
    CHECK(a.silent);
    CHECK(parse({ "--silent" }, a) == ARGS_OK);
    CHECK(a.silent);
}

TEST_CASE("options with required arguments") {
    struct arguments a;
    CHECK(parse({ "-o", "out.txt" }, a) == ARGS_OK);
    CHECK(std::strcmp(a.output_file, "out.txt") == 0);

    CHECK(parse({ "--output=out2.txt" }, a) == ARGS_OK);
    CHECK(std::strcmp(a.output_file, "out2.txt") == 0);

    CHECK(parse({ "--dap-port", "4711" }, a) == ARGS_OK);
    CHECK(std::strcmp(a.dap_port, "4711") == 0);

    CHECK(parse({ "-b", "EA" }, a) == ARGS_OK);
    CHECK(std::strcmp(a.break_opcode, "EA") == 0);
    CHECK(parse({ "--break=00" }, a) == ARGS_OK);
    CHECK(std::strcmp(a.break_opcode, "00") == 0);
}

TEST_CASE("crt optional argument") {
    struct arguments a;
    // Bare flag: enabled, no values.
    CHECK(parse({ "--crt" }, a) == ARGS_OK);
    CHECK(a.crt);
    CHECK(a.crt_values == nullptr);
    // Attached values (optional args require the =form).
    CHECK(parse({ "--crt=1,2,3" }, a) == ARGS_OK);
    CHECK(a.crt);
    CHECK(std::strcmp(a.crt_values, "1,2,3") == 0);
}

TEST_CASE("joystick optional argument defaults to digital_1") {
    struct arguments a;
    CHECK(parse({ "-j" }, a) == ARGS_OK);
    REQUIRE(a.joystick != nullptr);
    CHECK(std::strcmp(a.joystick, "digital_1") == 0);

    CHECK(parse({ "--joystick=digital_2" }, a) == ARGS_OK);
    CHECK(std::strcmp(a.joystick, "digital_2") == 0);
}

TEST_CASE("positional ROM; first one wins") {
    struct arguments a;
    CHECK(parse({ "--fullscreen", "game.xex" }, a) == ARGS_OK);
    CHECK(a.fullscreen);
    REQUIRE(a.rom != nullptr);
    CHECK(std::strcmp(a.rom, "game.xex") == 0);

    CHECK(parse({ "a.xex", "b.xex" }, a) == ARGS_OK);
    CHECK(std::strcmp(a.rom, "a.xex") == 0);
}

TEST_CASE("labels option invokes app_load_labels") {
    struct arguments a;
    g_labels_calls = 0;
    g_last_label_file = nullptr;
    CHECK(parse({ "--labels", "globals.lbl" }, a) == ARGS_OK);
    CHECK(g_labels_calls == 1);
    REQUIRE(g_last_label_file != nullptr);
    CHECK(std::strcmp(g_last_label_file, "globals.lbl") == 0);
}

TEST_CASE("help and version are reported, not exited") {
    struct arguments a;
    CHECK(parse({ "--help" }, a) == ARGS_SHOW_HELP);
    CHECK(parse({ "-h" }, a) == ARGS_SHOW_HELP);
    CHECK(parse({ "--version" }, a) == ARGS_SHOW_VERSION);
    CHECK(parse({ "-V" }, a) == ARGS_SHOW_VERSION);
}

TEST_CASE("unknown option yields an error with a message") {
    struct arguments a;
    const char* err = nullptr;
    CHECK(parse({ "--no-such-option" }, a, &err) == ARGS_ERROR);
    REQUIRE(err != nullptr);
    CHECK(std::strlen(err) > 0);  // must survive the parser's stack frame
}

// --- URL query -> argv builder (web build) ---------------------------------

// Convenience: collect a built argv into a vector<string> for easy assertions.
static std::vector<std::string> build(const char* query, const char* prog = "emu") {
    char** argv = args_build_argv_from_query(prog, query);
    std::vector<std::string> out;
    for (int i = 0; argv[i]; i++)
        out.emplace_back(argv[i]);
    return out;
}

TEST_CASE("query maps file to positional and options to --long") {
    auto v = build("?file=rom.xex&crt=1,2,3&fullscreen");
    REQUIRE(v.size() == 4);
    CHECK(v[0] == "emu");
    CHECK(v[1] == "rom.xex");       // file= -> bare positional
    CHECK(v[2] == "--crt=1,2,3");   // key=value -> --key=value
    CHECK(v[3] == "--fullscreen");  // bare key -> --key
}

TEST_CASE("leading question mark is optional") {
    CHECK(build("?fullscreen") == build("fullscreen"));
}

TEST_CASE("url-decoding of %XX and +") {
    auto v = build("file=my%20rom.xex&break=E%41");
    REQUIRE(v.size() == 3);
    CHECK(v[1] == "my rom.xex");
    CHECK(v[2] == "--break=EA");

    auto plus = build("file=a+b.xex");
    REQUIRE(plus.size() == 2);
    CHECK(plus[1] == "a b.xex");
}

TEST_CASE("tokens already in --long form pass through") {
    auto v = build("--fullscreen&--crt=1");
    REQUIRE(v.size() == 3);
    CHECK(v[1] == "--fullscreen");
    CHECK(v[2] == "--crt=1");
}

TEST_CASE("empty tokens and empty file= are skipped") {
    auto v = build("?&&fullscreen&file=&");
    REQUIRE(v.size() == 2);
    CHECK(v[0] == "emu");
    CHECK(v[1] == "--fullscreen");
}

TEST_CASE("empty and null queries yield just the program name") {
    CHECK(build("") == std::vector<std::string>{ "emu" });
    CHECK(build(nullptr) == std::vector<std::string>{ "emu" });
    CHECK(build("?") == std::vector<std::string>{ "emu" });
}

TEST_CASE("null program falls back to emu") {
    auto v = build("fullscreen", nullptr);
    REQUIRE(v.size() == 2);
    CHECK(v[0] == "emu");
}

TEST_CASE("end to end: query -> argv -> parsed arguments") {
    char** argv = args_build_argv_from_query("emu", "file=g.xex&fullscreen&crt=1,2");
    struct arguments a = args_defaults();
    REQUIRE(args_parse_argv(argv, &a, nullptr) == ARGS_OK);
    REQUIRE(a.rom != nullptr);
    CHECK(std::strcmp(a.rom, "g.xex") == 0);
    CHECK(a.fullscreen);
    CHECK(a.crt);
    REQUIRE(a.crt_values != nullptr);
    CHECK(std::strcmp(a.crt_values, "1,2") == 0);
}
