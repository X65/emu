#
# project: X65 emulator
#
cmake_minimum_required(VERSION 3.10)

project(emu VERSION 1.0 LANGUAGES C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(emu src/main.c)
