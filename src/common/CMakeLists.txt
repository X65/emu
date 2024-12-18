add_library(common STATIC)

add_compile_definitions(-DSOKOL_GLES3)
set(slang "glsl300es")
target_sources(common PRIVATE
    common.h
    clock.c
    fs.c
    gfx.c
    keybuf.c
    prof.c
    webapi.c
    common.h
    ui.cc
)

set(SHADER_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/shaders.glsl)
set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders.glsl.h)
add_custom_command(
    OUTPUT ${SHADER_OUTPUT}
    COMMAND ${SOKOL_SHDC_EXECUTABLE}
            --input ${SHADER_INPUT}
            --output ${SHADER_OUTPUT}
            --format sokol
            --slang ${slang}
    DEPENDS ${SHADER_INPUT}
    COMMENT "Compiling shaders with sokol-shdc"
)
add_custom_target(compile_shaders DEPENDS ${SHADER_OUTPUT})
add_dependencies(common compile_shaders)
target_include_directories(common PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_sources(common PRIVATE sokol.c)
target_link_libraries(common PRIVATE X11 Xcursor Xi EGL GLESv2 m dl asound)
target_compile_options(common BEFORE PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
