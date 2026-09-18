# Each CTest invocation owns its scripts, logs, settings, and output files.
file(MAKE_DIRECTORY "${TEST_DIR}")
set(ENV{ALSA_CONFIG_PATH} "${FIXTURE_DIR}/alsa-null.conf")
set(ENV{LIBGL_ALWAYS_SOFTWARE} "1")

# One X server is started by CTest for the whole script, so a test that runs the
# emulator twice does not pay for a second one.
function(run_emulator script expected_exit)
    execute_process(
        COMMAND "${EMU}" --disable-gui --zero-mem --seed 1
                --script "${script}" "${FIXTURE_DIR}/smoke.xex"
        WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30)
    get_filename_component(log_name "${script}" NAME)
    file(WRITE "${TEST_DIR}/${log_name}.log" "${output}\n${error}")
    if(NOT "${result}" STREQUAL "${expected_exit}")
        message(FATAL_ERROR "Emulator returned ${result}, expected ${expected_exit}:\n${output}\n${error}")
    endif()
    set(EMULATOR_OUTPUT "${output}\n${error}" PARENT_SCOPE)
endfunction()

if(TEST_KIND STREQUAL "Smoke")
    run_emulator("${FIXTURE_DIR}/smoke.scr" 0)
elseif(TEST_KIND STREQUAL "SeedRepeatability")
    foreach(run RANGE 1 2)
        set(DUMP_NAME "rng-${run}.bin")
        # Remove stale output so a previous successful run cannot hide a failure.
        file(REMOVE "${TEST_DIR}/${DUMP_NAME}")
        configure_file("${FIXTURE_DIR}/seed.scr.in" "${TEST_DIR}/seed-${run}.scr" @ONLY)
        run_emulator("${TEST_DIR}/seed-${run}.scr" 0)
        if(NOT EXISTS "${TEST_DIR}/${DUMP_NAME}")
            message(FATAL_ERROR "Missing RNG dump ${DUMP_NAME}")
        endif()
        file(SIZE "${TEST_DIR}/${DUMP_NAME}" dump_size)
        if(NOT dump_size EQUAL 32)
            message(FATAL_ERROR "RNG dump has ${dump_size} bytes, expected 32")
        endif()
    endforeach()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${TEST_DIR}/rng-1.bin" "${TEST_DIR}/rng-2.bin" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Seeded RNG dumps differ")
    endif()
elseif(TEST_KIND STREQUAL "WholeFrame")
    # A frame whose backdrop changed at line 120, captured two ways: by `run`,
    # which ends partway into the next frame, and by stopping in the vertical
    # blank after it.  Both must see the same split frame.  Each script ends on
    # a whole frame of the new colour, which must match too, and must not be
    # the split frame -- or the change never landed mid-frame and nothing was
    # compared.
    foreach(path run edge)
        run_emulator("${FIXTURE_DIR}/frame-${path}.scr" 0)
        string(REGEX MATCHALL "crc [0-9A-F]+" crcs "${EMULATOR_OUTPUT}")
        list(LENGTH crcs count)
        if(NOT count EQUAL 2)
            message(FATAL_ERROR "frame-${path}.scr printed ${count} CRCs, expected 2:\n${EMULATOR_OUTPUT}")
        endif()
        list(GET crcs 0 ${path}_split)
        list(GET crcs 1 ${path}_whole)
    endforeach()
    if(NOT edge_whole STREQUAL run_whole)
        message(FATAL_ERROR "The whole frames differ (${run_whole} vs ${edge_whole}): the two paths are not drawing the same thing")
    endif()
    if(edge_split STREQUAL edge_whole)
        message(FATAL_ERROR "The reference frame is not split (${edge_split}): the backdrop change did not land mid-frame")
    endif()
    if(NOT run_split STREQUAL edge_split)
        message(FATAL_ERROR "A capture after `run` is ${run_split}, the frame it finished is ${edge_split}: it caught the next frame drawing over it")
    endif()
elseif(TEST_KIND STREQUAL "CheckFailure")
    run_emulator("${FIXTURE_DIR}/failure.scr" 1)
    string(FIND "${EMULATOR_OUTPUT}" "$000300 is A5, expected 00" mismatch)
    if(mismatch EQUAL -1)
        message(FATAL_ERROR "Missing memory-mismatch diagnostic:\n${EMULATOR_OUTPUT}")
    endif()
else()
    message(FATAL_ERROR "Unknown script test kind: ${TEST_KIND}")
endif()
