# Based on https://www.mattkeeter.com/blog/2018-01-06-versioning/
#
# Stamps the emulator with its own revision. Every query names the repository
# explicitly with `git -C`, and the output path is passed in, because this runs
# with `cmake -P` and would otherwise describe whatever repository happens to
# enclose the working directory. That is not always this one: the X65 umbrella
# checkout builds the emulator into its own build tree, and a bare `git log`
# there reports the umbrella's HEAD - so the published emulator claimed a
# revision that was not an emulator commit, carried the umbrella's dirty flag,
# and was rebuilt on every unrelated umbrella commit.
#
# Expects: SRC_DIR (the emulator source tree) and OUT (the version.c to write).

execute_process(COMMAND git -C "${SRC_DIR}" log --pretty=format:%h -n 1
                OUTPUT_VARIABLE GIT_REV
                RESULT_VARIABLE GIT_RESULT
                ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if (NOT GIT_RESULT EQUAL 0 OR "${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
    set(GIT_DIFF "")
    set(GIT_TAG "N/A")
    set(GIT_BRANCH "N/A")
else()
    execute_process(
        COMMAND git -C "${SRC_DIR}" diff --quiet
        RESULT_VARIABLE GIT_DIRTY)
    set(GIT_DIFF "")
    if (NOT GIT_DIRTY EQUAL 0)
        set(GIT_DIFF "+")
    endif()
    execute_process(
        COMMAND git -C "${SRC_DIR}" describe --exact-match --tags
        OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process(
        COMMAND git -C "${SRC_DIR}" rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH)

    string(STRIP "${GIT_REV}" GIT_REV)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
endif()

set(VERSION "const char* GIT_REV=\"${GIT_REV}${GIT_DIFF}\";
const char* GIT_TAG=\"${GIT_TAG}\";
const char* GIT_BRANCH=\"${GIT_BRANCH}\";")

# Rewrite only on a real change, so an unchanged revision does not force a
# recompile of everything that links version.c.
if(EXISTS "${OUT}")
    file(READ "${OUT}" VERSION_)
else()
    set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
    file(WRITE "${OUT}" "${VERSION}")
endif()
