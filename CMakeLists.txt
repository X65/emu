#
# project: X65 emulator
#
cmake_minimum_required(VERSION 3.22)

project(emu VERSION 1.0 LANGUAGES C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(OpenGL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

include_directories(external/sokol)
include_directories(external/args/src)

add_executable(emu
    src/main.c
    src/cli.c
    ${CMAKE_CURRENT_BINARY_DIR}/version.c
    external/args/src/args.c
)
target_include_directories(emu PRIVATE ${READLINE_INCLUDE_DIRS})
target_link_libraries(emu
    PRIVATE ${READLINE_LIBRARIES}
    X11 Xi Xcursor EGL GLESv2 dl pthread m)
target_compile_options(emu PRIVATE ${READLINE_CFLAGS_OTHER})

# Add a custom command that produces version.c, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
           ${CMAKE_CURRENT_BINARY_DIR}/_version.c
    COMMAND ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)
