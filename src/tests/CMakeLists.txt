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

# Whole-machine execution, memory, interrupts, audio snapshots, and RNG.
# Link the same system/chips as the app with host globals in x65stubs.c.
add_executable(x65test x65test.cpp x65stubs.c ${X65_CORE_SOURCES})
# SDL3 comes in through ria816.c's gamepad plumbing; the tests never call it.
target_link_libraries(x65test PRIVATE speex-resampler SDL3::SDL3)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    # Three whole x65_t live here (two machines and a snapshot), ~53MB of static
    # state, and -sWASM_WORKERS pins us to INITIAL_MEMORY's 16MB default -- see
    # the same note on the emu target in the root CMakeLists.
    target_link_options(x65test PRIVATE "-sINITIAL_MEMORY=128MB")
endif()
add_test(NAME X65Test COMMAND x65test)

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

    # Executable integration needs the Linux display/audio backends. Portable
    # regressions remain in the native test targets on every platform.
    set(EMU_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/emu-smoke")
    find_program(XVFB_RUN NAMES xvfb-run)
    if(XVFB_RUN)
        message(STATUS "Emulator script tests enabled with ${XVFB_RUN}")
        foreach(kind Smoke SeedRepeatability CheckFailure)
            add_test(NAME EmuScript${kind}
                COMMAND "${XVFB_RUN}" -a -w 0 ${CMAKE_COMMAND}
                    "-DEMU=$<TARGET_FILE:emu>"
                    "-DFIXTURE_DIR=${EMU_FIXTURES}"
                    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/EmuScript${kind}"
                    "-DTEST_KIND=${kind}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/emu_script_test.cmake")
            set_tests_properties(EmuScript${kind} PROPERTIES TIMEOUT 90)
        endforeach()
    else()
        message(STATUS "Emulator script tests disabled: xvfb-run was not found (install Xvfb and xauth)")
    endif()

    # Both GUI tests are the same invocation; only the kind and the extra tool
    # paths differ. -w 0 skips xvfb-run's unconditional 3-second startup sleep.
    function(add_emu_gui_test name kind)
        add_test(NAME ${name}
            COMMAND ${CMAKE_COMMAND} -E env
                "ALSA_CONFIG_PATH=${EMU_FIXTURES}/alsa-null.conf"
                "LIBGL_ALWAYS_SOFTWARE=1"
                "XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/${name}/config"
                "${XVFB_RUN}" -a -w 0 -s "-screen 0 1280x1024x24"
                "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/emu_gui_test.py"
                --kind ${kind}
                "--emu=$<TARGET_FILE:emu>"
                "--fixture=${EMU_FIXTURES}/smoke.xex"
                "--xdotool=${XDOTOOL}"
                "--test-dir=${CMAKE_CURRENT_BINARY_DIR}/${name}"
                ${ARGN})
        # Generous, because the cap is only reached when a wait inside the
        # script is already timing out and printing its own diagnostics.
        set_tests_properties(${name} PROPERTIES TIMEOUT 120)
    endfunction()

    find_program(XDOTOOL NAMES xdotool)
    find_program(XPROP NAMES xprop)
    find_program(OPENBOX NAMES openbox)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(XVFB_RUN AND XDOTOOL AND Python3_Interpreter_FOUND)
        message(STATUS "Emulator joystick GUI test enabled with xdotool")
        add_emu_gui_test(EmuGuiJoystickInput joystick)
    else()
        message(STATUS "Emulator joystick GUI test disabled: requires xvfb-run, xdotool, and Python 3")
    endif()

    if(XVFB_RUN AND XDOTOOL AND XPROP AND OPENBOX AND Python3_Interpreter_FOUND)
        message(STATUS "Emulator window lifecycle test enabled with xdotool and Openbox")
        add_emu_gui_test(EmuGuiWindowLifecycle window "--xprop=${XPROP}" "--openbox=${OPENBOX}")
    else()
        message(STATUS "Emulator window lifecycle test disabled: additionally requires xprop and Openbox")
    endif()
endif()
