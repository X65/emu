add_executable(cputest cputest.cpp)
add_test(NAME CPUTest COMMAND cputest)

add_executable(argstest argstest.cpp ${CMAKE_SOURCE_DIR}/src/args.c)
add_test(NAME ArgsTest COMMAND argstest)

add_executable(rbtest rbtest.cpp ${CMAKE_SOURCE_DIR}/src/util/ringbuffer.c)
add_test(NAME RingBufferTest COMMAND rbtest)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(cpuemu cpuemu.c)
    add_test(NAME AllSuiteA COMMAND cpuemu -a 4000 ${CMAKE_CURRENT_SOURCE_DIR}/AllSuiteA.bin -r 4000 -d 0210 -w ${CMAKE_CURRENT_BINARY_DIR}/AllSuiteA.log)
    add_test(NAME AllSuiteA_log COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/AllSuiteA.log ${CMAKE_CURRENT_SOURCE_DIR}/AllSuiteA.log)
    set_tests_properties(AllSuiteA_log PROPERTIES FIXTURES_REQUIRED AllSuiteA)
endif()
