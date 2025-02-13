add_executable(cputest cputest.cpp)
add_test(NAME CPUTest COMMAND cputest)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(cpuemu cpuemu.c)
    # add_test(NAME CPUEmu COMMAND cpuemu -a 4000 AllSuiteA.bin)
endif()
