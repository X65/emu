#
# project: X65 emulator
#
cmake_minimum_required(VERSION 3.22)

project(emu VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

set(SOKOL_SHDC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol-tools-bin/bin/linux/sokol-shdc)

find_package(OpenGL REQUIRED)

add_subdirectory(ext)
include_directories(ext/imgui)
include_directories(ext/sokol)
include_directories(ext/sokol/util)
include_directories(ext)

include_directories(src)
add_subdirectory(src/common)

include_directories(src/common)
add_executable(emu
    src/chips/pwm.c
    src/chips/cgia.c
    src/chips/ria816.c
    src/chips/ymf262.c
    src/systems/x65.c
    src/x65.c
    src/x65-ui-impl.cc
    src/args.c
    src/ui/ui_cgia.cc
    src/ui/ui_console.cc
    src/ui/ui_ria816.cc
    src/ui/ui_ymf262.cc
    src/ui/ui_x65.cc
    src/util/ringbuffer.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)
target_link_libraries(emu
    PRIVATE common ui imgui-docking esfmu)

target_compile_definitions(emu PUBLIC CHIPS_USE_UI)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building for Linux")
    target_compile_definitions(emu PRIVATE USE_ARGP)
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
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s MALLOC='emmalloc'"
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

# Add a custom command that produces version.c, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
           ${CMAKE_CURRENT_BINARY_DIR}/_version.c
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)
