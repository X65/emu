#
# project: X65 emulator
#
cmake_minimum_required(VERSION 3.22)

project(emu VERSION 1.0 LANGUAGES C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

find_package(OpenGL REQUIRED)

include_directories(external/sokol)

add_executable(emu
    src/main.c
    src/cli.c
)
target_link_libraries(emu
    X11 Xi Xcursor EGL GLESv2 dl pthread m)
