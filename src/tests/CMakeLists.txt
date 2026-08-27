add_executable(cputest cputest.cpp)
add_test(NAME CPUTest COMMAND cputest)

add_executable(argstest argstest.cpp ${CMAKE_SOURCE_DIR}/src/args.c)
add_test(NAME ArgsTest COMMAND argstest)

add_executable(rbtest rbtest.cpp ${CMAKE_SOURCE_DIR}/src/util/ringbuffer.c)
add_test(NAME RingBufferTest COMMAND rbtest)

add_executable(sgu1test sgu1test.cpp ${CMAKE_SOURCE_DIR}/src/chips/sgu1.c)
target_link_libraries(sgu1test PRIVATE speex-resampler)
add_test(NAME SGU1Test COMMAND sgu1test)

# sgu1test stubs the core to test the register wrapper; this one links the real
# ext/sgu-1 core to cover the deferred reset and the read-to-clear status word.
add_executable(sgucoretest sgucoretest.cpp ${CMAKE_SOURCE_DIR}/ext/sgu-1/sgu.c)
add_test(NAME SGUCoreTest COMMAND sgucoretest)

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
    # CTest matches a fixture name against FIXTURES_SETUP, not against a test
    # name, so without this the two can race for the log file under ctest -j.
    set_tests_properties(AllSuiteA PROPERTIES FIXTURES_SETUP AllSuiteA)
    set_tests_properties(AllSuiteA_log PROPERTIES FIXTURES_REQUIRED AllSuiteA)

    # gilyon's 65816 instruction suite, built by src/tests/cputest816/ and
    # committed as a ROM so this needs no toolchain. -b because several tests
    # execute BRK; the harness plants STP at the handlers to catch stray ones.
    add_test(NAME CPU816Suite COMMAND cpuemu -a 8000 ${CMAKE_CURRENT_SOURCE_DIR}/cputest816.bin -o 7f01 -d 7f00 -b)
    set_tests_properties(CPU816Suite PROPERTIES
        PASS_REGULAR_EXPRESSION "Success"
        FAIL_REGULAR_EXPRESSION "Failed|Invalid test order"
        TIMEOUT 60)

    # WAI interrupt wake regression, built by src/tests/waitest/. -I wires up
    # cpuemu's interrupt request port, which is also what lets WAI run instead
    # of ending the simulation.
    add_test(NAME WaiInterrupt COMMAND cpuemu -a 8000 ${CMAKE_CURRENT_SOURCE_DIR}/waitest.bin -o 7f01 -d 7f00 -I 7f02)
    set_tests_properties(WaiInterrupt PROPERTIES
        PASS_REGULAR_EXPRESSION "WAI interrupt wake OK"
        FAIL_REGULAR_EXPRESSION "FAIL:"
        TIMEOUT 30)
endif()
