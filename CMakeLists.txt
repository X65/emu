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

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

add_subdirectory(ext)
include_directories(ext/imgui)
include_directories(ext/sokol)
include_directories(ext/sokol/util)
include_directories(ext)

include_directories(src)
add_subdirectory(src/common)

include_directories(src/common)
add_executable(emu
    src/chips/cgia.c
    src/chips/ria816.c
    src/systems/x65.c
    src/x65.c
    src/x65-ui-impl.cc
    src/args.c
    src/settings.cc
    src/ui/ui_cgia.cc
    src/ui/ui_console.cc
    src/ui/ui_x65.cc
    src/util/ringbuffer.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
)
target_include_directories(emu PRIVATE ${READLINE_INCLUDE_DIRS})
target_link_libraries(emu
    PRIVATE common ui imgui-docking
    ${READLINE_LIBRARIES})
target_compile_options(emu PRIVATE ${READLINE_CFLAGS_OTHER})

target_compile_definitions(emu PUBLIC CHIPS_USE_UI)

# Add a custom command that produces version.c, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
           ${CMAKE_CURRENT_BINARY_DIR}/_version.c
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)
