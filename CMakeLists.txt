# Minimum required CMake version
cmake_minimum_required(VERSION 3.10)

# Set the project name and language
project(MyCProject VERSION 1.0 LANGUAGES C)

# Specify the C standard (optional)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Specify the executable target
add_executable(MyCProject src/main.c)
