add_executable(cputest cputest.cpp)
add_test(NAME CPUTest COMMAND cputest)

add_executable(argstest argstest.cpp ${CMAKE_SOURCE_DIR}/src/args.c)
add_test(NAME ArgsTest COMMAND argstest)

add_executable(rbtest rbtest.cpp ${CMAKE_SOURCE_DIR}/src/util/ringbuffer.c)
add_test(NAME RingBufferTest COMMAND rbtest)

add_executable(sgu1test sgu1test.cpp ${CMAKE_SOURCE_DIR}/src/chips/sgu1.c)
target_link_libraries(sgu1test PRIVATE speex-resampler)
add_test(NAME SGU1Test COMMAND sgu1test)

add_executable(sst65816 sst65816.cpp)

# The SingleStepTests/65816 data set is ~3 GB of JSON and is not vendored;
# fetch it with tools/fetch-sst65816.sh. Configure with -DSST65816_DIR=<path>
# to the "v1" directory to register it as a test.
set(SST65816_DIR "" CACHE PATH "SingleStepTests/65816 v1 directory")
if(SST65816_DIR AND IS_DIRECTORY "${SST65816_DIR}")
    add_test(NAME SST65816 COMMAND sst65816 -s 4 -j 4 "${SST65816_DIR}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(cpuemu cpuemu.c)
    add_test(NAME AllSuiteA COMMAND cpuemu -a 4000 ${CMAKE_CURRENT_SOURCE_DIR}/AllSuiteA.bin -r 4000 -d 0210 -w ${CMAKE_CURRENT_BINARY_DIR}/AllSuiteA.log)
    add_test(NAME AllSuiteA_log COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/AllSuiteA.log ${CMAKE_CURRENT_SOURCE_DIR}/AllSuiteA.log)
    set_tests_properties(AllSuiteA_log PROPERTIES FIXTURES_REQUIRED AllSuiteA)
endif()
