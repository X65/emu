#
# project: X65 emulator
#
cmake_minimum_required(VERSION 3.22)

project(emu VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

if (NOT SOKOL_SHDC_EXECUTABLE)
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(_sokol_shdc_host win32)
    elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_sokol_shdc_host osx_arm64)
        else()
            set(_sokol_shdc_host osx)
        endif()
    else() # Linux and other Unix hosts
        if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(_sokol_shdc_host linux_arm64)
        else()
            set(_sokol_shdc_host linux)
        endif()
    endif()
    set(SOKOL_SHDC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol-tools-bin/bin/${_sokol_shdc_host}/sokol-shdc)
endif()

find_package(OpenGL REQUIRED)

# Must precede add_subdirectory(): audio runs on a Wasm AudioWorklet
# (src/common/sokol_audio_worklet.c), and -sAUDIO_WORKLET requires -sWASM_WORKERS,
# which implies shared memory. Shared memory is a wasm feature flag -- emcc turns it
# into -matomics -mbulk-memory on every compile, and wasm-ld rejects the link if any
# input object lacks them. So every target, ext/ libraries included, has to be built
# with it, which directory-scoped options only reach if they are set this early.
# -pthread is the flag that carries them: it also selects the "-mt" system libraries,
# where -sWASM_WORKERS alone would select the wasm-workers libc (libc-ww), which ships
# no single-threaded pthread stubs -- and libc++ (std::mutex, std::call_once,
# thread-local destructors) and SDL3's generic thread backend both reference them.
# Nothing here creates a pthread; -sWASM_WORKERS itself is emu-only and lives with
# -sAUDIO_WORKLET in the Emscripten block below.
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    add_compile_options(-pthread)
    add_link_options(-pthread)
endif()

add_subdirectory(ext)

# mingw-w64 has no `uint` (a glibc/BSD extension the chip headers share with the
# Pico firmware), so src/mingw.h supplies it. It has to be force-included into
# every target built from src/ -- emu and the test suites both compile
# X65_CORE_SOURCES -- hence a directory-scoped option here rather than a
# target_compile_options() on emu. Placed after add_subdirectory(ext) so the
# ext/ libraries, which build fine without it, stay untouched. SHELL: keeps the
# flag and its argument together -- src/common already force-includes config.h,
# and CMake would otherwise de-duplicate the second bare `-include` token.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_options("SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/mingw.h")
endif()

include_directories(ext/imgui ext/imgui_toggle)
include_directories(ext/sokol ext/sokol/util)
include_directories(ext/doctest/doctest)
include_directories(ext)

include_directories(src)
add_subdirectory(src/common)

include_directories(src/common)
include_directories(ext/firmware/src/south)
include_directories(ext/firmware/src/north)
include_directories(ext/firmware/src/audio)
include_directories(ext/firmware/src)
include_directories(ext/firmware/src/tinyusb/src)
# The emulated machine itself: the system wiring and every chip it ticks. Shared
# with the test suites in src/tests, which link the same machine without the host.
set(X65_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/chips/pwm.c
    ${CMAKE_SOURCE_DIR}/src/chips/cgia.c
    ${CMAKE_SOURCE_DIR}/src/chips/ria816.c
    ${CMAKE_SOURCE_DIR}/src/chips/sgu1.c
    ${CMAKE_SOURCE_DIR}/src/chips/tca6416a.c
    ${CMAKE_SOURCE_DIR}/src/systems/x65.c
    ${CMAKE_SOURCE_DIR}/src/util/ringbuffer.c
    ${CMAKE_SOURCE_DIR}/ext/sgu-1/sgu.c
)

add_executable(emu
    ${X65_CORE_SOURCES}
    src/x65.c
    src/x65-ui-impl.cc
    src/args.c
    src/hid.c
    src/script.c
    src/ui/ui_cgia.cc
    src/ui/ui_vram_debugger.cc
    src/ui/ui_console.cc
    src/ui/ui_ria816.cc
    src/ui/ui_sgu1.cc
    src/ui/ui_tca6416a.cc
    src/ui/ui_app_log.cc
    src/ui/ui_x65.cc
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)
target_link_libraries(emu
    PRIVATE common ui
            imgui-docking imgui-toggle speex-resampler
            SDL3::SDL3)

target_compile_definitions(emu PUBLIC CHIPS_USE_UI)
target_compile_definitions(emu PRIVATE ${SOKOL_GFX_BACKEND_DEFINE})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building for Linux")
    set(CPPDAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ext/cppdap")
    add_subdirectory(${CPPDAP_DIR})
    target_compile_definitions(emu PRIVATE USE_DAP)
    target_sources(emu PRIVATE src/dap.cc)
    target_link_libraries(emu PRIVATE cppdap)

    find_library(UNWIND_LIB unwind)
    find_library(UNWIND_X86_64_LIB unwind-x86_64)
    if (UNWIND_X86_64_LIB)
        target_compile_definitions(emu PRIVATE USE_UNWIND)
        target_sources(emu PRIVATE src/crash.cc)
        target_link_libraries(emu PRIVATE unwind unwind-x86_64)
    endif()
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building for Windows (mingw-w64)")
    target_compile_options(emu BEFORE PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(emu PRIVATE --static)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Building for Emscripten (WASM)")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_compile_definitions(emu PRIVATE USE_WEB)
    target_link_libraries(emu PRIVATE
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/src/shell.html"
        "-s DISABLE_EXCEPTION_CATCHING=1"
        "-s ASSERTIONS=0"
        "-s STACK_SIZE=512KB"
        "-s ERROR_ON_UNDEFINED_SYMBOLS=1"
        # growth with shared memory makes JS-side heap views refresh on every
        # access; the hot paths (emulation, audio render) are pure Wasm, so accept
        # that rather than guessing a fixed heap size
        "-Wno-pthreads-mem-growth"
        "-s ALLOW_MEMORY_GROWTH=1"
        # -sWASM_WORKERS forces IMPORTED_MEMORY, which rules out INITIAL_HEAP and
        # falls back to INITIAL_MEMORY -- whose 16MB default is far under the ~279MB
        # of static state in `state` (8 snapshot slots, each a whole x65_t with its
        # 16MB ram[]). Without this wasm-ld fails with "initial memory too small".
        # Grow this if `state` in src/x65.c grows.
        "-s INITIAL_MEMORY=320MB"
        # a *growable shared* memory makes engines reserve the declared maximum of
        # address space up front, and the default maximum is 2GB -- more than a
        # 32-bit or mobile browser will hand out. 1GB still leaves ~700MB of growth.
        "-s MAXIMUM_MEMORY=1GB"
        "-s MALLOC='emmalloc'"
        "-s WASM_WORKERS=1"
        "-s AUDIO_WORKLET=1"
        # we synchronise through Wasm memory, so drop the AudioParam marshalling
        "-s AUDIO_WORKLET_SUPPORT_AUDIO_PARAMS=0"
        "-s NO_FILESYSTEM=1"
        "-s WASM=1"
        -O3
        -flto
        "--closure 1"
    )
    target_compile_options(emu PRIVATE
        -sDISABLE_EXCEPTION_CATCHING=1
        -fno-exceptions
        -fno-rtti
        -flto
        -O3
        -DNDEBUG
    )
endif()

include(CTest)
enable_testing()
add_subdirectory(src/tests)

# Add a custom command that produces version.c, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
           ${CMAKE_CURRENT_BINARY_DIR}/_version.c
    COMMAND ${CMAKE_COMMAND}
            -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/version.c
            -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
    VERBATIM)
