#
# project: X65 emulator
#
cmake_minimum_required(VERSION 3.22)

project(emu VERSION 1.0 LANGUAGES C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(SOKOL_SHDC_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/ext/sokol-tools-bin/bin/linux/sokol-shdc)

find_package(OpenGL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

include_directories(ext/sokol)
include_directories(ext/sokol/util)
include_directories(ext/chips)
include_directories(ext/chips-test/examples/common)
include_directories(ext)

add_subdirectory(ext)

include_directories(src)
add_executable(emu
    src/main.c
    src/cli.c
    src/cmd.c
    src/chips/ria816.c
    src/chips/cgia.c
    src/chips/ymf825.c
    src/chips/mcp23017.c
    src/systems/x65.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)
target_include_directories(emu PRIVATE ${READLINE_INCLUDE_DIRS})
target_link_libraries(emu
    PRIVATE ext
    ${READLINE_LIBRARIES})
target_compile_options(emu PRIVATE ${READLINE_CFLAGS_OTHER})

# Add a custom command that produces version.c, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
           ${CMAKE_CURRENT_BINARY_DIR}/_version.c
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)
